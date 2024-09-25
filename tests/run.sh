#!/bin/bash
source "$(dirname "$0")/util.sh"


echo -e "\e[34mInit server and wait 3 seconds\e[0m"
./build/bialet -p 7000 -l /tmp/tests.log $(dirname "$0")&
sleep 3

# Tests
run_test "Get the URL parameter" "get-url-param?foo=bar" 200 "bar"
run_test "Get the post parameter" "post-param" "foo=bar" 200 "bar"
run_test "Get the route parameter" "route/baz/qux" 200 "bazqux"
run_test "This URL not exists" "donotexists" 404
run_test "Parsing error" "parsing-error" 500

# Print the summary and exit with error if any tests failed
print_summary >&2

killall bialet

exit $?
