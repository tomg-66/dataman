import Dataman.Dataman;
import Dataman.DatamanIndex;

/** Live server transaction visibility and rollback of client buffers. */
public final class TransactionIntegrationTest {
	private static void check(boolean result, String message) {
		if (!result) throw new AssertionError(message);
	}
	public static void main(String[] args) {
		Dataman.initDataman("TransactionIntegrationTest",
			new String[]{"-n", "-h", args[1], "-r", args[0], "unused"});
		DatamanIndex index = new DatamanIndex("one_rec_idx", DatamanIndex.UPDATE);
		check(index.get_first(), "baseline");
		Dataman.startTransaction();
		index.insert(1, DatamanIndex.AFTER);
		Dataman.master.field[1].putString("1234567");
		index.include(index, Dataman.master.field[1]);
		check(index.get("1234567"), "read own insert");
		Dataman.master.field[1].putString("discard");
		Dataman.rollback();
		check(!index.get("1234567"), "rolled-back key remains");
		check(index.get_first(), "baseline after rollback");
		Dataman.startTransaction();
		index.insert(1, DatamanIndex.AFTER);
		Dataman.master.field[1].putString("2345678");
		index.include(index, Dataman.master.field[1]);
		check(Dataman.commit(), "commit");
		check(index.get("2345678"), "committed insert missing");
		Dataman.startTransaction();
		index.delete();
		Dataman.rollback();
		check(index.get("2345678"), "rollback did not restore deletion");
		index.iclose();
		System.out.println("live transactions: PASS");
	}
}
