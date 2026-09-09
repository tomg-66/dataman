--TEST--
dataman assigns master fields and returns exact current-record metadata
--EXTENSIONS--
dataman
--FILE--
<?php
var_dump(dataman_connect(['-r', '/home/tomg/dataman/tests/php', '-h', 'localhost']));
var_dump(dataman_iopen('blob_rec_idx', UPDATE));
var_dump(dataman_get_first('blob_rec_idx'));
var_dump(dataman_get_format());
var_dump(dataman_get_index());
var_dump(dataman_get_file());
var_dump(dataman_key_str());
var_dump(bin2hex(dataman_get_key()));
$masterRecord[2] = 'master';
var_dump($masterRecord[2]);
$masterRecord[3] = '';
var_dump($masterRecord[3]);
$masterRecord[3] = "\x00persist\xff";
var_dump(bin2hex($masterRecord[3]));
foreach ([0, 4] as $field) {
    try {
        var_dump($masterRecord[$field]);
    } catch (Throwable $e) {
        echo get_class($e), "\n";
    }
}
var_dump(dataman_iclose('blob_rec_idx'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
int(1)
string(12) "blob_rec_idx"
string(8) "blob_rec"
string(7) "BLOB001"
string(32) "424c4f4230303101000000000000001e"
string(9) "master   "
string(0) ""
string(18) "0070657273697374ff"
ValueError
ValueError
bool(true)
