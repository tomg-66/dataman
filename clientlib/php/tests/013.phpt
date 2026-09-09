--TEST--
dataman assigns work fields and replaces empty and binary work blobs
--EXTENSIONS--
dataman
--ARGS--
-p -7 -h localhost -r /home/tomg/dataman/tests/php blob_rec_idx blob_rec
--FILE--
<?php
$testDirectory = realpath(__DIR__ . '/../../../tests');
$setupScript = $testDirectory . '/setup_test.sh';
exec('cd ' . escapeshellarg($testDirectory) . ' && ' . escapeshellarg($setupScript) . ' php 2>&1', $setupOutput, $setupStatus);
if ($setupStatus !== 0) {
    throw new RuntimeException("Fixture setup failed:\n" . implode("\n", $setupOutput));
}
var_dump(dataman_mkidx());
do {
    $workRecord[2] = 'work';
    var_dump($workRecord[2]);
    $workRecord[3] = "\x00blob\xffdata";
    var_dump(bin2hex($workRecord[3]));
    $workRecord[3] = '';
    var_dump($workRecord[3]);
    foreach ([0, 4] as $field) {
        try {
            $workRecord[$field] = 'invalid';
        } catch (Throwable $e) {
            echo get_class($e), "\n";
        }
    }
    var_dump(dataman_sort($workRecord[1]));
} while (dataman_release());
?>
--EXPECT--
bool(true)
string(9) "work     "
string(20) "00626c6f62ff64617461"
string(0) ""
ValueError
ValueError
bool(true)
