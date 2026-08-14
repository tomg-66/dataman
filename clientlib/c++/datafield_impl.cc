/*
 * Dataman C++ datafield implementation.
 *
 * Record fields are fixed-width values owned by masterRecord or workRecord.
 * Standalone fields, including arithmetic results, resize as needed and do
 * not mark either record dirty.
 */

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "datafield.hpp"
#include "datarecord.hpp"
#include "fileEdit.hpp"
#include "datamanError.hpp"

#include "../../server/errors.h"

using namespace Dataman;

namespace {

struct number {
	double value;
	bool integral;
};

static int trimmed_length(const char *text)
{
	if (!text)
		return 0;

	int length = (int)std::strlen(text);
	while (length > 0 && text[length - 1] == ' ')
		--length;
	return length;
}

static bool parse_number(const char *text, number& result)
{
	if (!text)
		return false;

	char *end;
	errno = 0;
	double value = std::strtod(text, &end);
	if (end == text || errno == ERANGE || !std::isfinite(value))
		return false;

	while (*end == ' ' || *end == '\t' || *end == '\n' ||
			*end == '\r' || *end == '\f' || *end == '\v')
		++end;
	if (*end != '\0')
		return false;

	result.value = value;
	result.integral = std::strpbrk(text, ".eE") == NULL &&
			value >= INT_MIN && value <= INT_MAX;
	return true;
}

static number require_number(const datafield& field)
{
	if (field.get_type() == type_blob)
		throw makeError(EBLOBTYP, "datafield arithmetic");

	number value;
	if (!parse_number(field.getptr(), value))
		throw datamanError(0, "datafield value is not numeric");
	return value;
}

static number require_number(const char *text)
{
	number value;
	if (!parse_number(text, value))
		throw datamanError(0, "datafield value is not numeric");
	return value;
}

static datafield make_number(double value, bool integral)
{
	datafield result;
	if (integral && value >= INT_MIN && value <= INT_MAX)
		result = (int)value;
	else
		result = (float)value;
	return result;
}

static std::string field_text(const datafield& field)
{
	return std::string(field.getptr(), (size_t)trimmed_length(field.getptr()));
}

static datafield concatenate(const datafield& left, const char *right,
		int right_length)
{
	std::string value = field_text(left);
	if (right && right_length > 0)
		value.append(right, (size_t)right_length);
	return datafield(value.c_str());
}

static datafield concatenate(const datafield& left, const datafield& right)
{
	return concatenate(left, right.getptr(), trimmed_length(right.getptr()));
}

static void reject_blob(const datafield& field)
{
	if (field.get_type() == type_blob)
		throw makeError(EBLOBTYP, "datafield operation");
}

} // namespace

bool datafield::is_bound() const
{
	return which == MASTER || which == WORK;
}

void datafield::mark_dirty()
{
	if (which == MASTER)
		masterRecord.setdirty(true);
	else if (which == WORK)
		workRecord.setdirty(true);
}

void datafield::assign(const char *source, int source_length,
		fieldTypes source_type)
{
	if (source_length < 0)
		source_length = 0;

	if (is_bound()) {
		if (type == type_blob)
			throw makeError(EBLOBTYP, "assignment to blob field");
		if (!data) {
			data = new char[(size_t)length + 1];
		}
		std::memset(data, ' ', (size_t)length);
		if (source && source_length > 0) {
			int copy_length = source_length < length ? source_length : length;
			std::memcpy(data, source, (size_t)copy_length);
		}
		data[length] = '\0';
		type = type_chr;
		mark_dirty();
		return;
	}

	delete[] data;
	length = source ? source_length : 0;
	data = new char[(size_t)length + 1];
	if (source && length > 0)
		std::memcpy(data, source, (size_t)length);
	data[length] = '\0';
	type = source_type;
}

datafield::datafield(void)
	: length(0), type(type_non), data(NULL), which(standalone)
{
}

datafield::datafield(const char *text, int offset, int requested_length)
	: length(0), type(type_non), data(NULL), which(standalone)
{
	if (!text)
		return;

	int text_length = (int)std::strlen(text);
	if (offset < 0 || offset > text_length)
		throw datamanError(0, "datafield string offset is out of range");

	int available = text_length - offset;
	int copy_length = requested_length == 0 ? available : requested_length;
	if (copy_length < 0)
		throw datamanError(0, "datafield string length is invalid");
	if (copy_length > available)
		copy_length = available;
	assign(text + offset, copy_length, type_chr);
}

datafield::datafield(const datafield& source)
	: length(source.length), type(source.type), data(NULL), which(standalone)
{
	if (source.type == type_blob) {
		if (length > 0) {
			data = new char[(size_t)length];
			std::memcpy(data, source.data, (size_t)length);
		}
	} else if (source.data) {
		data = new char[(size_t)length + 1];
		std::memcpy(data, source.data, (size_t)length);
		data[length] = '\0';
	}
}

datafield::~datafield()
{
	delete[] data;
}

datafield& datafield::operator=(const datafield& source)
{
	if (this == &source)
		return *this;

	if (source.type == type_blob) {
		if (is_bound() && type != type_blob)
			throw makeError(EBLOBTYP, "blob assignment to text field");
		if (!is_bound())
			which = standalone;
		delete[] data;
		length = source.length;
		data = length > 0 ? new char[(size_t)length] : NULL;
		if (length > 0)
			std::memcpy(data, source.data, (size_t)length);
		type = type_blob;
		mark_dirty();
		return *this;
	}

	if (is_bound() && type == type_blob)
		throw makeError(EBLOBTYP, "text assignment to blob field");
	assign(source.getptr(), source.length, source.type);
	return *this;
}

datafield& datafield::operator=(const char *text)
{
	assign(text, text ? (int)std::strlen(text) : 0, type_chr);
	return *this;
}

datafield& datafield::operator=(int value)
{
	char buffer[32];
	int size = std::snprintf(buffer, sizeof(buffer), "%d", value);
	assign(buffer, size, type_int);
	return *this;
}

datafield& datafield::operator=(float value)
{
	char buffer[64];
	int size = std::snprintf(buffer, sizeof(buffer), "%f", value);
	assign(buffer, size, type_flt);
	return *this;
}

void datafield::make_field(const char *source, int field_length, int owner)
{
	if (field_length < 0)
		throw datamanError(0, "text field length is invalid");

	delete[] data;
	which = owner;
	length = field_length;
	type = type_chr;
	data = new char[(size_t)length + 1];
	if (source && length > 0)
		std::memcpy(data, source, (size_t)length);
	else if (length > 0)
		std::memset(data, ' ', (size_t)length);
	data[length] = '\0';
}

void datafield::make_blob_field(const void *source, int blob_length, int owner)
{
	if (blob_length < 0 || (!source && blob_length > 0))
		throw datamanError(0, "blob field length or data is invalid");

	delete[] data;
	which = owner;
	length = blob_length;
	type = type_blob;
	data = length > 0 ? new char[(size_t)length] : NULL;
	if (length > 0)
		std::memcpy(data, source, (size_t)length);
}

int datafield::put_blob(const void *source, int blob_length)
{
	if ((type != type_blob && type != type_non) || blob_length < 0 ||
			(!source && blob_length > 0))
		return 0;

	delete[] data;
	length = blob_length;
	data = length > 0 ? new char[(size_t)length] : NULL;
	if (length > 0)
		std::memcpy(data, source, (size_t)length);
	type = type_blob;
	mark_dirty();
	return 1;
}

datafield datafield::operator+(const datafield& right) const
{
	reject_blob(*this);
	reject_blob(right);

	bool left_numeric_type = type == type_int || type == type_flt;
	bool right_numeric_type = right.type == type_int || right.type == type_flt;
	if (!left_numeric_type && !right_numeric_type)
		return concatenate(*this, right);

	number left;
	number rhs;
	if (!parse_number(getptr(), left) || !parse_number(right.getptr(), rhs))
		return concatenate(*this, right);
	return make_number(left.value + rhs.value,
			left.integral && rhs.integral && type != type_flt &&
			right.type != type_flt);
}

datafield datafield::operator+(const char *right) const
{
	reject_blob(*this);
	if (!right)
		return *this;
	if (type != type_int && type != type_flt)
		return concatenate(*this, right, (int)std::strlen(right));

	number left;
	number rhs;
	if (!parse_number(getptr(), left) || !parse_number(right, rhs))
		return concatenate(*this, right, (int)std::strlen(right));
	return make_number(left.value + rhs.value,
			left.integral && rhs.integral && type != type_flt);
}

datafield datafield::operator+(int right) const
{
	reject_blob(*this);
	number left;
	if (!parse_number(getptr(), left)) {
		char buffer[32];
		int size = std::snprintf(buffer, sizeof(buffer), "%d", right);
		return concatenate(*this, buffer, size);
	}
	return make_number(left.value + right, left.integral && type != type_flt);
}

datafield datafield::operator+(float right) const
{
	reject_blob(*this);
	number left;
	if (!parse_number(getptr(), left)) {
		char buffer[64];
		int size = std::snprintf(buffer, sizeof(buffer), "%f", right);
		return concatenate(*this, buffer, size);
	}
	return make_number(left.value + right, false);
}

datafield datafield::operator*(const datafield& right) const
{
	number left = require_number(*this);
	number rhs = require_number(right);
	return make_number(left.value * rhs.value,
			left.integral && rhs.integral && type != type_flt &&
			right.type != type_flt);
}

datafield datafield::operator*(const char *right) const
{
	number left = require_number(*this);
	number rhs = require_number(right);
	return make_number(left.value * rhs.value,
			left.integral && rhs.integral && type != type_flt);
}

datafield datafield::operator*(int right) const
{
	number left = require_number(*this);
	return make_number(left.value * right, left.integral && type != type_flt);
}

datafield datafield::operator*(float right) const
{
	number left = require_number(*this);
	return make_number(left.value * right, false);
}

datafield datafield::operator/(const datafield& right) const
{
	number left = require_number(*this);
	number rhs = require_number(right);
	if (rhs.value == 0.0)
		throw datamanError(0, "attempt to divide datafield by zero");
	if (left.integral && rhs.integral && type != type_flt &&
			right.type != type_flt)
		return make_number((int)left.value / (int)rhs.value, true);
	return make_number(left.value / rhs.value, false);
}

datafield datafield::operator/(const char *right) const
{
	number left = require_number(*this);
	number rhs = require_number(right);
	if (rhs.value == 0.0)
		throw datamanError(0, "attempt to divide datafield by zero");
	if (left.integral && rhs.integral && type != type_flt)
		return make_number((int)left.value / (int)rhs.value, true);
	return make_number(left.value / rhs.value, false);
}

datafield datafield::operator/(int right) const
{
	if (right == 0)
		throw datamanError(0, "attempt to divide datafield by zero");
	number left = require_number(*this);
	if (left.integral && type != type_flt)
		return make_number((int)left.value / right, true);
	return make_number(left.value / right, false);
}

datafield datafield::operator/(float right) const
{
	if (right == 0.0f)
		throw datamanError(0, "attempt to divide datafield by zero");
	number left = require_number(*this);
	return make_number(left.value / right, false);
}

bool datafield::operator==(const datafield& right) const
{
	reject_blob(*this);
	reject_blob(right);
	return std::strcmp(getptr(), right.getptr()) == 0;
}

bool datafield::operator==(const char *right) const
{
	reject_blob(*this);
	if (!right)
		return false;
	if (std::strcmp(right, " ") == 0)
		return trimmed_length(getptr()) == 0;
	return std::strcmp(getptr(), right) == 0;
}

bool datafield::operator==(int right) const
{
	reject_blob(*this);
	number left;
	return parse_number(getptr(), left) && left.value == right;
}

bool datafield::operator==(float right) const
{
	reject_blob(*this);
	number left;
	return parse_number(getptr(), left) && left.value == right;
}

bool datafield::operator!=(const datafield& right) const
{
	return !(*this == right);
}

bool datafield::operator!=(const char *right) const
{
	return !(*this == right);
}

bool datafield::operator!=(int right) const
{
	return !(*this == right);
}

bool datafield::operator!=(float right) const
{
	return !(*this == right);
}

const char *strcpy(datafield& destination, const char *source)
{
	destination = source;
	return destination.getptr();
}

char *strcpy(char *destination, datafield& source)
{
	return ::strcpy(destination, source.getptr());
}

char *strncpy(char *destination, datafield& source, int length)
{
	return ::strncpy(destination, source.getptr(), (size_t)length);
}

const char *Dataman::strncpy(datafield& destination, const char *source,
		int length)
{
	if (!source || length <= 0)
		return destination.getptr();
	if (destination.type == type_blob)
		throw makeError(EBLOBTYP, "strncpy to blob field");

	int copy_length = length < destination.length ? length : destination.length;
	int source_length = (int)std::strlen(source);
	if (copy_length > source_length)
		copy_length = source_length;
	if (copy_length > 0)
		std::memcpy(destination.data, source, (size_t)copy_length);
	if (destination.data)
		destination.data[destination.length] = '\0';
	destination.mark_dirty();
	return destination.getptr();
}

char *strcat(char *destination, datafield& source)
{
	return ::strcat(destination, source.getptr());
}

char *strncat(char *destination, datafield& source, int length)
{
	return ::strncat(destination, source.getptr(), (size_t)length);
}

int atoi(datafield& source)
{
	return std::atoi(source.getptr());
}

const void *Dataman::memcpy(datafield& destination, const char *source,
		int length)
{
	if (!source || length <= 0)
		return destination.getptr();
	int copy_length = length < destination.length ? length : destination.length;
	if (copy_length > 0)
		std::memcpy(destination.data, source, (size_t)copy_length);
	if (destination.type != type_blob && destination.data)
		destination.data[destination.length] = '\0';
	destination.mark_dirty();
	return destination.getptr();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
