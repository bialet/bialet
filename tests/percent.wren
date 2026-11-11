// A bare percent is a literal in a Wren string; only "%(" starts
// interpolation.
var name = "world"

return "100% " + "50%|%(name)|%(1 + 1)%|\%("
