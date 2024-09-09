#!/bin/bash

# Script to display a directory structure.
# If .gitignore exists, it shows the structure of files respecting .gitignore using 'tree --fromfile'.
# If .gitignore is missing, it displays the directory structure using 'tree' with predefined options.
# Accepts an optional argument for the directory to display. Defaults to the current directory.
# Requires 'tree' command to be installed.

# Directory to process
DIR="${1:-.}"

# Check if tree command is available
if ! command -v tree &> /dev/null; then
    echo "tree command not found. Please install it to use this feature."
    exit 1
fi

# Function to create a list of files respecting .gitignore
generate_file_list() {
    git -C "$DIR" ls-files --others --cached --exclude-standard --full-name |
    grep -v -e '^\.git/' |
    sort
}

# Function to create a list of all files and directories, ignoring .gitignore
generate_all_file_list() {
    find "$DIR" -type f -not -path '*/\.*' |
    sort
}

# Change to the specified directory
cd "$DIR" || { echo "Failed to change directory to '$DIR'"; exit 1; }

# Check if .gitignore is present
if [ -f ".gitignore" ]; then
    # Generate the file list respecting .gitignore
    file_list=$(generate_file_list)
    if [ -n "$file_list" ]; then
        echo "$file_list" | tree --fromfile
    else
        echo "No files to display."
    fi
else
    # Use tree command directly if .gitignore is not present
    tree -L 3 --charset=ascii --noreport -P "*" -I ".git|.gitignore|README.md" --prune
fi
