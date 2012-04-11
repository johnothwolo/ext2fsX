#!/bin/sh
cc $@ -F/Library/Frameworks \
-framework Cocoa -framework ExtFSDiskManager \
-g -o extmgr test_main.m
