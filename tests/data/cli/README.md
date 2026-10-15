# Command-line tool fixtures

Designs for the tests of `QtRocket_cli` (`tests/core/cli/CliTests.cpp`, run in the test process,
and `tests/core/cli/CMakeLists.txt`, which runs the executable and checks its exit codes). Both
were written by hand for QtRocket as plain XML, so that a reader sees what they hold. The
examples of `data/examples/` cover everything else the tests run.

| File | What it is | What the tool does with it |
|---|---|---|
| `no-motors.ork` | A design of file version 1.10: a nose cone and a body tube with fins and a motor mount, a flight configuration without motors and one simulation, "Without a motor", of it. | `simulate` runs it, and the flight ends in its first step with a `SIM_ABORT` event, "No motors defined in the simulation", an ordinary abort: exit code 3, with the summary printed and the CSV written all the same. `info` prints it (two configurations: the rocket's default one and the file's). |
| `malformed.ork` | The first 24 lines of `no-motors.ork`: XML that ends inside an element. | The loader refuses it ("Malformed XML in input."): exit code 2. |

The simulation leaves the random seed to the tool and needs no motor of the database, so the
abort comes the same with any seed, time step or motor database.
