--TEST--
dataman repeatedly opens and closes an index while preserving blob replacements
--EXTENSIONS--
dataman
--FILE--
<?php
var_dump(dataman_connect(['-r', '/home/tomg/dataman/tests/php', '-h', 'localhost']));
var_dump(dataman_iopen('blob_rec_idx', RDONLY));
var_dump(dataman_get_first('blob_rec_idx'));
var_dump($masterRecord[2]);
var_dump(bin2hex($masterRecord[3]));
var_dump(dataman_iclose('blob_rec_idx'));
var_dump(dataman_iopen('blob_rec_idx', UPDATE));
var_dump(dataman_get_first('blob_rec_idx'));
$masterRecord[3] = '';
var_dump($masterRecord[3]);
var_dump(dataman_iclose('blob_rec_idx'));
var_dump(dataman_iopen('blob_rec_idx', RDONLY));
var_dump(dataman_get_first('blob_rec_idx'));
var_dump($masterRecord[2]);
var_dump($masterRecord[3]);
var_dump(dataman_iclose('blob_rec_idx'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
string(9) "master   "
string(18) "0070657273697374ff"
bool(true)
bool(true)
bool(true)
string(0) ""
bool(true)
bool(true)
bool(true)
string(9) "master   "
string(0) ""
bool(true)
