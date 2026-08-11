import Dataman.Dataman;
import Dataman.DatamanIndex;
import Dataman.DatamanRuntimeException;

/** Exercises the same mutation sequence as PHP tests 011 and 012. */
public final class OneRecordIntegrationTest {
	private static final String INDEX_NAME = "one_rec_idx";

	private OneRecordIntegrationTest() {
	}

	private static void check(boolean condition, String message) {
		if (!condition)
			throw new AssertionError(message);
	}

	private static void checkField(int field, String expected) {
		String actual = Dataman.master.field[field].getString();
		check(expected.equals(actual),
			"field " + field + ": expected <" + expected +
			"> but got <" + actual + ">");
	}

	private static void mutationTest() {
		DatamanIndex index = new DatamanIndex(INDEX_NAME, DatamanIndex.UPDATE);
		check(index.get_first(), "initial record was not indexed");
		checkField(1, "0024817");

		boolean refusedOnlyRecordDelete = false;
		try {
			index.delete();
		} catch (DatamanRuntimeException expected) {
			refusedOnlyRecordDelete = true;
		}
		check(refusedOnlyRecordDelete,
			"deleting the only record should have failed");
		checkField(1, "0024817");

		index.insert(2, DatamanIndex.AFTER);
		Dataman.master.field[2].putString("this is a test");
		checkField(2, "thi");

		index.insert(1, DatamanIndex.AFTER);
		Dataman.master.field[1].putString("0131572");
		Dataman.master.field[3].putString("88888888");
		checkField(1, "0131572");
		checkField(3, "888");
		checkField(2, "         ");

		index.include(index, Dataman.master.field[1]);
		check(index.back(), "could not move back to the format-2 record");
		index.delete();
		index.iclose();
		System.out.println("mutation: PASS");
	}

	private static void automaticRemovalTest() {
		DatamanIndex index = new DatamanIndex(INDEX_NAME, DatamanIndex.UPDATE);
		check(index.get("0131572"), "included key was not found");
		checkField(1, "0131572");
		index.delete();
		check(!index.get_current(),
			"key pointing to the deleted record was not removed");
		index.iclose();
		System.out.println("automatic-removal: PASS");
	}

	public static void main(String[] args) {
		if (args.length != 2)
			throw new IllegalArgumentException(
				"usage: OneRecordIntegrationTest database-root host");

		String[] datamanArgs = {
			"-n", "-h", args[1], "-r", args[0], "unused"
		};
		Dataman.initDataman("OneRecordIntegrationTest", datamanArgs);
		mutationTest();
		automaticRemovalTest();
	}
}
