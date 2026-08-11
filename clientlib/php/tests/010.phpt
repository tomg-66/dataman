--TEST--
dataman builds an index from the one-record fixture
--EXTENSIONS--
dataman
--ARGS--
-p -7 -h localhost -r /home/tomg/dataman/tests/php one_rec_idx one_rec
--FILE--
<?php
$testDirectory = realpath(__DIR__ . '/../../../tests');
$setupScript = $testDirectory . '/setup_test.sh';

exec(
    'cd ' . escapeshellarg($testDirectory)
    . ' && ' . escapeshellarg($setupScript) . ' php 2>&1',
    $setupOutput,
    $setupStatus
);

if ($setupStatus !== 0) {
    throw new RuntimeException(
        "Fixture setup failed:\n" . implode("\n", $setupOutput)
    );
}

var_dump(dataman_mkidx());

do {
    var_dump(dataman_sort($workRecord[1]));
} while (dataman_release());
?>
--EXPECT--
bool(true)
bool(true)
