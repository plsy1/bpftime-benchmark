# Invalid/incomplete run

This directory is not a benchmark result and must not be used for conclusions.

The runner rejected the environment before collecting round 1 because CPU5
was no longer locked: min/current frequency had fallen to 729.6 MHz while max
remained 1.728 GHz. Exit status was 1 and no timing summary was produced.

`sudo jetson_clocks` was re-applied and min/max/current were all verified at
1.728 GHz before starting the next fresh run.
