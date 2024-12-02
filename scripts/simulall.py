#!/usr/bin/env python

import simulate
import sys

def main():
    datadir = sys.argv[1]
    simulate.main(datadir, "pa", False, "_no_delay")
    simulate.main(datadir, "pa", True, "_delay")

    simulate.main(datadir, "ffa", False, "_no_delay")
    simulate.main(datadir, "ffa", True, "_delay")
    simulate.main(datadir, "ffa_timer", True)

    simulate.main(datadir, "csf", False, "_no_delay")
    simulate.main(datadir, "csf", True, "_delay")
    simulate.main(datadir, "csf_timer", True)


if __name__ == "__main__":
    main()
