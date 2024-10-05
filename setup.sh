#!/bin/bash
rm config.txt
#TODO: Add verification for parameters
cat >> config.txt << EOF
$1
$2
EOF
