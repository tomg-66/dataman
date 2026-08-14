#include <cassert>
#include <cmath>
#include <cstring>

#include "datafield.hpp"
#include "datarecord.hpp"
#include "datamanError.hpp"
#include "endSort.hpp"
#include "dataman.hpp"
#include "sort.hpp"

using namespace Dataman;

static void expect_text(const datafield& field, const char *expected)
{
	assert(std::strcmp(field.getptr(), expected) == 0);
}

static void assignment_tests()
{
	datafield value;
	value = "a";
	expect_text(value, "a");
	value = "a longer value";
	expect_text(value, "a longer value");
	assert(value.datalen() == 14);

	datafield chained;
	chained = value = "chain";
	expect_text(value, "chain");
	expect_text(chained, "chain");

	workRecord.setdirty(false);
	datafield field;
	field.make_field("abcde", 5, WORK);
	field = "xy";
	expect_text(field, "xy   ");
	assert(field.datalen() == 5);
	assert(field.get_type() == type_chr);
	assert(workRecord.getdirty());

	workRecord.setdirty(false);
	datafield copy(field);
	copy = "standalone can grow";
	expect_text(copy, "standalone can grow");
	assert(!workRecord.getdirty());

	field = "123456";
	expect_text(field, "12345");
	field = 12;
	expect_text(field, "12   ");
	assert(field.get_type() == type_chr);
}

static void addition_tests()
{
	datafield ten("10");
	datafield twenty("20");
	expect_text(ten + twenty, "1020");
	expect_text(ten + 20, "30");
	expect_text(datafield("abc") + 20, "abc20");

	datafield numeric;
	numeric = 10;
	expect_text(numeric + twenty, "30");
	expect_text(numeric + datafield("x"), "10x");

	datafield decimal("10.5");
	datafield sum = decimal + 2;
	assert(std::fabs(std::atof(sum.getptr()) - 12.5) < 0.0001);
}

static void arithmetic_tests()
{
	expect_text(datafield("6") * datafield("7"), "42");
	expect_text(datafield("7") / 2, "3");

	datafield quotient = datafield("7.5") / 2;
	assert(std::fabs(std::atof(quotient.getptr()) - 3.75) < 0.0001);

	bool threw = false;
	try {
		(void)(datafield("not numeric") * 2);
	} catch (const datamanError&) {
		threw = true;
	}
	assert(threw);

	threw = false;
	try {
		(void)(datafield("4") / 0);
	} catch (const datamanError&) {
		threw = true;
	}
	assert(threw);
}

static void blob_tests()
{
	const unsigned char bytes[] = {0x01, 0x00, 0xff};
	datafield blob;
	assert(blob.put_blob(bytes, sizeof(bytes)) == 1);
	datafield copy(blob);
	assert(copy.get_type() == type_blob);
	assert(copy.datalen() == (int)sizeof(bytes));
	assert(std::memcmp(copy.getptr(), bytes, sizeof(bytes)) == 0);

	datafield empty_blob;
	empty_blob.make_blob_field(NULL, 0, WORK);
	assert(empty_blob.get_type() == type_blob);
	assert(empty_blob.datalen() == 0);
}

int main()
{
	assignment_tests();
	addition_tests();
	arithmetic_tests();
	blob_tests();
	return 0;
}
