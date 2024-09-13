#!/bin/bash

# CPU Frequency Scaling Management Script
#
# This script manages CPU frequency scaling settings on Linux systems.
# It can save current settings, restore previous settings, modify CPU
# frequency configurations, set optimized defaults, and view current settings.
#
# Usage:
#   ./manage_cpu_scaling.sh save                                  Save current CPU scaling settings
#   ./manage_cpu_scaling.sh restore                               Restore saved CPU scaling settings
#   ./manage_cpu_scaling.sh set <governor> [freq]                 Set CPU governor and optionally max frequency
#   ./manage_cpu_scaling.sh set optimized                         Set optimized CPU scaling settings to non-turbo base frequency
#   ./manage_cpu_scaling.sh view                                  View current CPU scaling settings
#
# Parameters:
#   save                                  Saves current CPU scaling settings to backup files
#   restore                               Restores CPU scaling settings from backup files
#   set <governor> [freq]                 Sets CPU governor and optionally max frequency
#     <governor>: 'performance', 'powersave', 'ondemand', etc. - CPU governor to set
#     [freq]: Optional. Frequency value to set (e.g., 2.5GHz), defaults to base frequency if not specified
#   set optimized                         Sets CPU governor to performance with the non-turbo base frequency
#   view                                  Displays current CPU scaling settings
#
# Examples:
#   ./manage_cpu_scaling.sh save
#   ./manage_cpu_scaling.sh set performance
#   ./manage_cpu_scaling.sh set ondemand
#   ./manage_cpu_scaling.sh set optimized
#   ./manage_cpu_scaling.sh view
#   ./manage_cpu_scaling.sh restore
#
# Note: Modifying CPU scaling settings requires root privileges. Use sudo if necessary.

# --- Configuration ---
BACKUP_DIR="/tmp/cpu_scaling_backup"  # Directory to store backup files
ORIGINAL_SETTINGS_FLAG="$BACKUP_DIR/.original_settings"  # Flag file to indicate original settings have been saved

# --- Functions ---

# Function to get the base frequency of the CPU
get_base_frequency() {
    local base_freq_file="/sys/devices/system/cpu/cpu0/cpufreq/base_frequency"
    local cpuinfo_file="/proc/cpuinfo"

    if [ -f "$base_freq_file" ]; then
        cat "$base_freq_file"
    elif [ -f "$cpuinfo_file" ]; then
        # Try to get the CPU frequency from /proc/cpuinfo
        grep -m1 "model name" "$cpuinfo_file" | sed -n 's/.*@ \(.*\)GHz.*/\1/p' | awk '{print $1*1000000}'
    else
        echo "Error: Unable to determine base frequency"
        return 1
    fi
}

# Function to get the allowed frequency range
get_allowed_freq_range() {
    local cpu_path=$1
    local min_freq="$(cat "$cpu_path/cpufreq/scaling_min_freq")"
    local max_freq="$(cat "$cpu_path/cpufreq/scaling_max_freq")"
    echo "$min_freq $max_freq"
}

# Function to save current CPU scaling settings
save_cpu_scaling_settings() {
    if [ -f "$ORIGINAL_SETTINGS_FLAG" ]; then
        echo "Original settings have already been saved. To avoid overwriting, this save operation will be skipped."
        echo "To view current settings, use the 'view' command."
        return
    fi

    echo "Saving current CPU scaling settings as original settings..."
    mkdir -p "$BACKUP_DIR"
    for cpu_path in /sys/devices/system/cpu/cpu[0-9]*; do
        cpu_num=$(basename "$cpu_path" | sed 's/cpu//')
        governor_file="$cpu_path/cpufreq/scaling_governor"
        freq_file="$cpu_path/cpufreq/scaling_max_freq"

        if [ -f "$governor_file" ]; then
            cat "$governor_file" > "$BACKUP_DIR/governor_$cpu_num"
        fi
        if [ -f "$freq_file" ]; then
            cat "$freq_file" > "$BACKUP_DIR/max_freq_$cpu_num"
        fi
    done
    touch "$ORIGINAL_SETTINGS_FLAG"
    echo "Original settings saved to $BACKUP_DIR."
}

# Function to restore CPU scaling settings
restore_cpu_scaling_settings() {
    if [ ! -f "$ORIGINAL_SETTINGS_FLAG" ]; then
        echo "No original settings found to restore. Please save settings first."
        return
    fi

    echo "Restoring original CPU scaling settings..."
    echo "Summary of original settings to be restored:"
    echo "-----------------------------------"

    for cpu_path in /sys/devices/system/cpu/cpu[0-9]*; do
        cpu_num=$(basename "$cpu_path" | sed 's/cpu//')
        governor_file="$BACKUP_DIR/governor_$cpu_num"
        freq_file="$BACKUP_DIR/max_freq_$cpu_num"

        if [ -f "$governor_file" ]; then
            governor=$(cat "$governor_file")
            echo "CPU$cpu_num Governor: $governor"
        else
            echo "CPU$cpu_num Governor: No backup found"
        fi

        if [ -f "$freq_file" ]; then
            freq_mhz=$(($(cat "$freq_file") / 1000))
            echo "CPU$cpu_num Max Frequency: ${freq_mhz}MHz"
        else
            echo "CPU$cpu_num Max Frequency: No backup found"
        fi
    done

    echo "-----------------------------------"
    echo "Applying original settings..."

    for cpu_path in /sys/devices/system/cpu/cpu[0-9]*; do
        cpu_num=$(basename "$cpu_path" | sed 's/cpu//')
        governor_file="$cpu_path/cpufreq/scaling_governor"
        freq_file="$cpu_path/cpufreq/scaling_max_freq"

        if [ -f "$BACKUP_DIR/governor_$cpu_num" ]; then
            sudo sh -c "cat $BACKUP_DIR/governor_$cpu_num > $governor_file" || echo "Failed to restore governor for CPU$cpu_num"
        fi
        if [ -f "$BACKUP_DIR/max_freq_$cpu_num" ]; then
            sudo sh -c "cat $BACKUP_DIR/max_freq_$cpu_num > $freq_file" || echo "Failed to restore max frequency for CPU$cpu_num"
        fi
    done

    echo "Original settings restored from $BACKUP_DIR."
    echo "Current settings after restoration:"
    view_cpu_scaling_settings
}

# Function to view current CPU scaling settings
view_cpu_scaling_settings() {
    echo "Current CPU Scaling Settings:"
    echo "-----------------------------"
    for cpu_path in /sys/devices/system/cpu/cpu[0-9]*; do
        cpu_num=$(basename "$cpu_path" | sed 's/cpu//')
        governor_file="$cpu_path/cpufreq/scaling_governor"
        freq_file="$cpu_path/cpufreq/scaling_max_freq"

        if [ -f "$governor_file" ]; then
            echo "CPU$cpu_num Governor: $(cat "$governor_file")"
        fi
        if [ -f "$freq_file" ]; then
            freq_mhz=$(($(cat "$freq_file") / 1000))
            echo "CPU$cpu_num Max Frequency: ${freq_mhz}MHz"
        fi
    done
}

# Function to set and verify CPU scaling settings
set_and_verify_cpu_scaling_settings() {
    local governor=$1
    local freq=$2
    local valid_governors="performance powersave ondemand userspace conservative schedutil"

    # Check if original settings have been saved, if not, create them
    if [ ! -f "$ORIGINAL_SETTINGS_FLAG" ]; then
        echo "Original settings backup not found. Creating a backup of current settings as original..."
        save_cpu_scaling_settings
    fi

    # Set CPU governor
    if [[ "$valid_governors" =~ (^|[[:space:]])"$governor"($|[[:space:]]) ]]; then
        for gov_file in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor; do
            echo "$governor" | sudo tee "$gov_file" > /dev/null
        done
    else
        echo "Invalid governor. Valid options are: $valid_governors"
        exit 1
    fi

    # Set CPU frequency if provided
    if [ -n "$freq" ] && [ "$freq" != "Error: Unable to determine base frequency" ]; then
        echo "Attempting to set CPU frequency to $freq Hz..."
        for cpu_path in /sys/devices/system/cpu/cpu[0-9]*; do
            cpu_num=$(basename "$cpu_path" | sed 's/cpu//')
            freq_file="$cpu_path/cpufreq/scaling_max_freq"

            if [ -f "$freq_file" ]; then
                read -r min_freq max_freq <<< "$(get_allowed_freq_range "$cpu_path")"

                if [ "$freq" -ge "$min_freq" ] && [ "$freq" -le "$max_freq" ]; then
                    echo "$freq" | sudo tee "$freq_file" > /dev/null
                else
                    echo "Warning: Frequency $freq Hz is outside the allowed range ($min_freq - $max_freq Hz) for CPU$cpu_num"
                    echo "Setting to the closest allowed value..."
                    if [ "$freq" -lt "$min_freq" ]; then
                        echo "$min_freq" | sudo tee "$freq_file" > /dev/null
                    else
                        echo "$max_freq" | sudo tee "$freq_file" > /dev/null
                    fi
                fi
            else
                echo "Warning: Frequency scaling not available for CPU$cpu_num"
            fi
        done
    else
        echo "Warning: Unable to set CPU frequency. Using current maximum frequency."
    fi

    echo "Verifying applied settings..."
    view_cpu_scaling_settings
}

# Function to set optimized CPU scaling settings
set_optimized_cpu_scaling() {
    local freq
    freq=$(get_base_frequency)

    echo "Setting optimized CPU scaling settings (performance governor at base frequency)..."
    set_and_verify_cpu_scaling_settings "performance" "$freq"
}

# --- Main Script ---
case "$1" in
    save)
        save_cpu_scaling_settings
        ;;
    restore)
        restore_cpu_scaling_settings
        ;;
    set)
        if [ "$2" = "optimized" ]; then
            set_optimized_cpu_scaling
        elif [ "$#" -lt 2 ]; then
            echo "Usage: $0 set <governor> [max_freq]"
            echo "   or: $0 set optimized"
            exit 1
        else
            set_and_verify_cpu_scaling_settings "$2" "$3"
        fi
        ;;
    view)
        view_cpu_scaling_settings
        ;;
    *)
        echo "Usage: $0 {save|restore|set <governor> [max_freq]|set optimized|view}"
        echo "  governor: 'performance', 'powersave', 'ondemand', 'userspace', 'conservative', 'schedutil'"
        echo "  max_freq: optional maximum frequency value (e.g., 2500000 for 2.5GHz)"
        exit 1
esac

exit 0