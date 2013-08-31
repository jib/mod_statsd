<?php
    # $x || $y always returns 1...
    $r = $_GET['resp'] ? $_GET['resp'] : 200;

    # This is a fancy new function (o.O) in php 5.4
    # we'll have to do it the old fashioned way
    #http_response_code( $r );

    header('X-PHP-Response-Code: ' . $r, true, $r );
?>
