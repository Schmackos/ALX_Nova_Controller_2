#!/bin/bash
# Helper script to amend the last commit with updated release notes
# Usage: ./update-release-notes.sh

echo "Amending commit with release notes..."
git commit --amend --no-edit

if [ $? -eq 0 ]; then
    echo "✓ Commit amended successfully with release notes"
else
    echo "✗ Failed to amend commit"
    exit 1
fi
