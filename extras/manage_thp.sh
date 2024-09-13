#!/bin/bash

# Transparent Huge Pages (THP) Management Script
#
# This script manages Transparent Huge Pages (THP) settings on Linux systems.
# It can save current settings, restore previous settings, modify THP
# configurations, set optimized defaults, and view current settings.
#
# Usage:
#   ./manage_thp.sh save                                  Save current THP settings
#   ./manage_thp.sh restore                               Restore saved THP settings
#   ./manage_thp.sh set <enabled> <defrag> <pages>        Set THP mode, defrag, and number of pages
#   ./manage_thp.sh set optimized [pages]                 Set optimized THP settings with optional page count
#   ./manage_thp.sh view                                  View current THP settings
#
# Parameters:
#   save                                  Saves current THP settings to backup files
#   restore                               Restores THP settings from backup files
#   set <enabled> <defrag> <pages>        Sets THP mode, defrag, and number of huge pages
#     <enabled>: 'always', 'madvise', or 'never' - THP enabled state
#     <defrag>: 'always', 'defer', 'defer+madvise', or 'never' - THP defrag state
#     <pages>: Positive integer - Number of huge pages to allocate
#   set optimized [pages]                 Sets THP to optimized defaults (always, defer+madvise)
#     [pages]: Optional. Positive integer for number of huge pages
#   view                                  Displays current THP settings
#
# Examples:
#   ./manage_thp.sh save
#   ./manage_thp.sh set always defer 2048
#   ./manage_thp.sh set madvise defer+madvise 1024
#   ./manage_thp.sh set optimized
#   ./manage_thp.sh set optimized 2048
#   ./manage_thp.sh view
#   ./manage_thp.sh restore
#
# Note: Modifying THP settings requires root privileges. Use sudo if necessary.

# Function to save current THP settings
save_thp_settings() {
    if [ -f "$ORIGINAL_SETTINGS_FLAG" ]; then
        echo "Original settings have already been saved. To avoid overwriting, this save operation will be skipped."
        echo "To view current settings, use the 'view' command."
        return
    fi

    echo "Saving current THP settings as original settings..."
    mkdir -p "$BACKUP_DIR"
    cat /sys/kernel/mm/transparent_hugepage/enabled > "$BACKUP_DIR/thp_enabled_backup"
    cat /sys/kernel/mm/transparent_hugepage/defrag > "$BACKUP_DIR/thp_defrag_backup"
    cat /proc/sys/vm/nr_hugepages > "$BACKUP_DIR/hugepages_backup"
    touch "$ORIGINAL_SETTINGS_FLAG"
    echo "Original settings saved to $BACKUP_DIR"
}

# Function to restore THP settings
restore_thp_settings() {
    if [ ! -f "$ORIGINAL_SETTINGS_FLAG" ]; then
        echo "No original settings found to restore. Please save settings first."
        return
    fi

    echo "Restoring original THP settings..."
    if [ -f "$BACKUP_DIR/thp_enabled_backup" ] && [ -f "$BACKUP_DIR/thp_defrag_backup" ] && [ -f "$BACKUP_DIR/hugepages_backup" ]; then
        local enabled=$(cat "$BACKUP_DIR/thp_enabled_backup")
        local defrag=$(cat "$BACKUP_DIR/thp_defrag_backup")
        local pages=$(cat "$BACKUP_DIR/hugepages_backup")

        echo "Summary of original settings to be restored:"
        echo "-----------------------------------"
        echo "THP Enabled: $enabled"
        echo "THP Defrag: $defrag"
        echo "Number of Huge Pages: $pages"
        echo "-----------------------------------"
        echo "Applying original settings..."

        set_and_verify_thp_settings "$enabled" "$defrag" "$pages"
    else
        echo "Backup files not found. Cannot restore settings."
    fi
}

# Function to view current THP settings
view_thp_settings() {
    echo "Current THP Settings:"
    echo "---------------------"
    echo "THP Enabled: $(cat /sys/kernel/mm/transparent_hugepage/enabled)"
    echo "THP Defrag: $(cat /sys/kernel/mm/transparent_hugepage/defrag)"
    echo "Number of Huge Pages: $(cat /proc/sys/vm/nr_hugepages)"
}

# Function to verify THP settings
verify_thp_settings() {
    local intended_enabled=$1
    local intended_defrag=$2
    local intended_pages=$3
    local actual_enabled=$(cat /sys/kernel/mm/transparent_hugepage/enabled)
    local actual_defrag=$(cat /sys/kernel/mm/transparent_hugepage/defrag)
    local actual_pages=$(cat /proc/sys/vm/nr_hugepages)
    local mismatch=false

    if [[ "$actual_enabled" != *"[$intended_enabled]"* ]]; then
        echo "Warning: THP Enabled setting mismatch. Intended: $intended_enabled, Actual: $actual_enabled"
        mismatch=true
    fi
    if [[ "$actual_defrag" != *"[$intended_defrag]"* ]]; then
        echo "Warning: THP Defrag setting mismatch. Intended: $intended_defrag, Actual: $actual_defrag"
        mismatch=true
    fi
    if [ "$actual_pages" -ne "$intended_pages" ]; then
        echo "Warning: Number of Huge Pages mismatch. Intended: $intended_pages, Actual: $actual_pages"
        mismatch=true
    fi

    if [ "$mismatch" = true ]; then
        return 1
    else
        return 0
    fi
}

# Function to set and verify THP settings
set_and_verify_thp_settings() {
    local thp_setting=$1
    local defrag_setting=$2
    local num_pages=$3

    # Check if original settings have been saved, if not, create them
    if [ ! -f "$ORIGINAL_SETTINGS_FLAG" ]; then
        echo "Original settings backup not found. Creating a backup of current settings as original..."
        save_thp_settings
    fi

    # Set THP enabled
    if [[ "$thp_setting" =~ ^(always|madvise|never)$ ]]; then
        sudo sh -c "echo $thp_setting > /sys/kernel/mm/transparent_hugepage/enabled"
    else
        echo "Invalid THP setting. Use 'always', 'madvise', or 'never'."
        exit 1
    fi

    # Set THP defrag
    if [[ "$defrag_setting" =~ ^(always|defer|defer\+madvise|never)$ ]]; then
        sudo sh -c "echo $defrag_setting > /sys/kernel/mm/transparent_hugepage/defrag"
    else
        echo "Invalid defrag setting. Use 'always', 'defer', 'defer+madvise', or 'never'."
        exit 1
    fi

    # Set number of hugepages
    if [[ "$num_pages" =~ ^[0-9]+$ ]]; then
        echo $num_pages | sudo tee /proc/sys/vm/nr_hugepages > /dev/null
    else
        echo "Invalid number of pages. Please provide a positive integer."
        exit 1
    fi

    echo "Verifying applied settings..."
    if verify_thp_settings "$thp_setting" "$defrag_setting" "$num_pages"; then
        echo "All settings applied successfully."
    else
        echo "Some settings may not have been applied correctly. Please check the warnings above."
    fi

    view_thp_settings
}

# Function to set optimized THP settings
set_optimized_thp() {
    local num_pages=${1:-$(cat /proc/sys/vm/nr_hugepages)}  # Use current pages if not specified

    echo "Setting optimized THP settings..."
    set_and_verify_thp_settings "always" "defer+madvise" "$num_pages"
}

# Main script
case "$1" in
    save)
        save_thp_settings
        ;;
    restore)
        restore_thp_settings
        ;;
    set)
        if [ "$2" = "optimized" ]; then
            set_optimized_thp "$3"
        elif [ "$#" -lt 4 ]; then
            echo "Usage: $0 set <thp_setting> <defrag_setting> <num_pages>"
            echo "   or: $0 set optimized [num_pages]"
            exit 1
        else
            set_and_verify_thp_settings "$2" "$3" "$4"
        fi
        ;;
    view)
        view_thp_settings
        ;;
    *)
        echo "Usage: $0 {save|restore|set <thp_setting> <defrag_setting> <num_pages>|set optimized [num_pages]|view}"
        echo "  thp_setting: 'always', 'madvise', or 'never'"
        echo "  defrag_setting: 'always', 'defer', 'defer+madvise', or 'never'"
        echo "  num_pages: positive integer for number of hugepages"
        exit 1
esac

exit 0