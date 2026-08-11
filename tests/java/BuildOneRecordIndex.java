import Dataman.Dataman;
import Dataman.DatamanIndex;

/** Builds the index used by the Java integration tests. */
public final class BuildOneRecordIndex {
	private BuildOneRecordIndex() {
	}

	public static void main(String[] args) {
		if (args.length != 2)
			throw new IllegalArgumentException(
				"usage: BuildOneRecordIndex database-root host");

		String[] datamanArgs = {
			"-7", "-h", args[1], "-r", args[0],
			"one_rec_idx", "one_rec"
		};
		DatamanIndex index = Dataman.makeIndex(
			"BuildOneRecordIndex", datamanArgs);

		do {
			index.sort(Dataman.workfile.field[1]);
		} while (Dataman.workfile.release());

		System.out.println("build-index: PASS");
		/* The Dataman shutdown hook publishes and closes the completed index. */
	}
}
