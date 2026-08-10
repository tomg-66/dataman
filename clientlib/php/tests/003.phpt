--TEST--
dataman_get validates its arguments
--EXTENSIONS--
dataman
--FILE--
<?php
foreach ([
    fn() => dataman_get(),
    fn() => dataman_get("index", []),
    fn() => dataman_get("index", str_repeat("x", 65)),
] as $test) {
    try {
        $test();
    } catch (Throwable $e) {
        echo get_class($e), "\n";
    }
}
?>
--EXPECT--
ArgumentCountError
TypeError
ValueError

