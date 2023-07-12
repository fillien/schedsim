#!/bin/bash

barectf generate --metadata-dir=trace src/config.yaml
mv barectf.c barectf.h barectf-bitfield.h src/
