#!/bin/bash
if [ ! -f "docs/html/index.html" ]; then
    echo "Documentation not found! Generating..."
    doxygen
fi

echo "Opening File Documentation in Browser using wslview"
wslview docs/html/index.html
echo "Done! Viewing Documentation in Browser!"
