# Rockchip pinctrl numbers each 32-pin bank consecutively: GPIO4_A6 = 134.
# Accept only an explicit free GPIO and free mux; unknown output fails closed.
$1 == "pin" && $2 == "134" && $3 == "(gpio4-6):" {
    print
    hits++
    if ($0 ~ /: UNCLAIMED[[:space:]]*$/ ||
        $0 ~ /: \(MUX UNCLAIMED\) \(GPIO UNCLAIMED\)[[:space:]]*$/)
        free_pins++
}
END {
    if (hits != 1) {
        print "STOP: expected exactly one GPIO4_A6 ownership entry; cannot confirm pin availability."
        exit 1
    }
    if (free_pins != 1) {
        print "STOP: GPIO4_A6 is claimed. Keep the key disconnected and share this output."
        exit 1
    }
    print "GPIO4_A6: mux and GPIO are unclaimed."
}
