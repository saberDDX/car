# LubanCat-3: physical pin 5 = GPIO3_A0/SCL, pin 3 = GPIO3_A1/SDA.
$1 == "pin" && (($2 == "96" && $3 == "(gpio3-0):") ||
               ($2 == "97" && $3 == "(gpio3-1):")) {
    print
    hits[$2]++
    if ($0 ~ /: UNCLAIMED[[:space:]]*$/ ||
        $0 ~ /: \(MUX UNCLAIMED\) \(GPIO UNCLAIMED\)[[:space:]]*$/)
        free_pins[$2]++
}
END {
    failed = 0
    for (pin = 96; pin <= 97; pin++) {
        if (hits[pin] != 1 || free_pins[pin] != 1) {
            print "STOP: pin " pin " is claimed or its ownership cannot be confirmed."
            failed = 1
        }
    }
    if (failed)
        exit 1
    print "GPIO3_A0/SCL and GPIO3_A1/SDA: mux and GPIO are unclaimed."
}
