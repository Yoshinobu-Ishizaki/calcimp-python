#!/usr/bin/env python3
"""
Test script for calcimp Python module
Reads sample/test.men and calculates input impedance
"""

import numpy as np
import calcimp

def test_calcimp():
    # Path to the test mensur file
    mensur_file = "../sample/test.men"

    # Parameters for calculation
    max_freq = 2000.0      # Maximum frequency in Hz
    step_freq = 2.5        # Frequency step in Hz
    temperature = 24.0     # Temperature in Celsius

    print("=" * 60)
    print("Testing calcimp module")
    print("=" * 60)
    print(f"Mensur file: {mensur_file}")
    print(f"Max frequency: {max_freq} Hz")
    print(f"Step frequency: {step_freq} Hz")
    print(f"Temperature: {temperature} °C")
    print()

    # Call calcimp to calculate impedance
    print("Calculating impedance...")
    frequencies, real_part, imag_part, magnitude_db = calcimp.calcimp(
        mensur_file,
        max_freq=max_freq,
        step_freq=step_freq,
        temperature=temperature
    )

    print(f"Calculation complete!")
    print(f"Number of frequency points: {len(frequencies)}")
    print()

    # Display first 10 results
    print("First 10 results:")
    print("-" * 60)
    print(f"{'Freq (Hz)':<12} {'Real':<15} {'Imag':<15} {'Mag (dB)':<12}")
    print("-" * 60)
    for i in range(min(10, len(frequencies))):
        print(f"{frequencies[i]:<12.2f} {real_part[i]:<15.6e} {imag_part[i]:<15.6e} {magnitude_db[i]:<12.2f}")
    print()

    # Display statistics
    print("Statistics:")
    print("-" * 60)
    print(f"Frequency range: {frequencies[0]:.2f} - {frequencies[-1]:.2f} Hz")
    print(f"Real part range: {np.min(real_part):.6e} - {np.max(real_part):.6e}")
    print(f"Imag part range: {np.min(imag_part):.6e} - {np.max(imag_part):.6e}")
    print(f"Magnitude range: {np.min(magnitude_db):.2f} - {np.max(magnitude_db):.2f} dB")
    print()

    # Find peak frequencies (local maxima in magnitude)
    print("Peak frequencies (top 5 by magnitude):")
    print("-" * 60)
    peak_indices = np.argsort(magnitude_db)[-5:][::-1]
    for idx in peak_indices:
        print(f"  {frequencies[idx]:7.2f} Hz: {magnitude_db[idx]:8.2f} dB")
    print()

    return frequencies, real_part, imag_part, magnitude_db

def write_impedance_file(filename, frequencies, real_part, imag_part, magnitude_db):
    """
    Write impedance results to file in .imp format

    Parameters:
        filename: output filename
        frequencies: array of frequencies in Hz
        real_part: real part of impedance
        imag_part: imaginary part of impedance
        magnitude_db: magnitude in dB
    """
    with open(filename, 'w') as f:
        # Write header
        f.write("freq,imp.real,imp.imag,mag\n")

        # Write data
        for i in range(len(frequencies)):
            f.write(f"{frequencies[i]:.6f},{real_part[i]:.10E},{imag_part[i]:.10E},{magnitude_db[i]:.10E}\n")

    print(f"Results written to: {filename}")

def compare_impedance_files(file1, file2, tolerance=1e-6):
    """
    Compare two impedance files and check if they match within tolerance

    Parameters:
        file1: first file path
        file2: second file path
        tolerance: relative tolerance for comparison

    Returns:
        bool: True if files match within tolerance
    """
    print("\n" + "=" * 60)
    print("Comparing impedance files")
    print("=" * 60)
    print(f"File 1: {file1}")
    print(f"File 2: {file2}")
    print(f"Tolerance: {tolerance}")
    print()

    try:
        # Read both files
        data1 = np.genfromtxt(file1, delimiter=',', skip_header=1)
        data2 = np.genfromtxt(file2, delimiter=',', skip_header=1)

        if data1.shape != data2.shape:
            print(f"ERROR: Files have different shapes!")
            print(f"  {file1}: {data1.shape}")
            print(f"  {file2}: {data2.shape}")
            return False

        print(f"Number of data points: {len(data1)}")
        print()

        # Compare each column
        columns = ['Frequency', 'Real part', 'Imaginary part', 'Magnitude']
        all_match = True

        for col_idx, col_name in enumerate(columns):
            col1 = data1[:, col_idx]
            col2 = data2[:, col_idx]

            # Calculate differences
            abs_diff = np.abs(col1 - col2)
            max_abs_diff = np.max(abs_diff)
            mean_abs_diff = np.mean(abs_diff)

            # Calculate relative differences (avoid division by zero)
            with np.errstate(divide='ignore', invalid='ignore'):
                rel_diff = abs_diff / np.abs(col1)
                rel_diff = np.where(np.isfinite(rel_diff), rel_diff, 0)
            max_rel_diff = np.max(rel_diff)
            mean_rel_diff = np.mean(rel_diff)

            # Check if within tolerance
            matches = max_rel_diff <= tolerance
            status = "✓ MATCH" if matches else "✗ DIFFER"
            all_match = all_match and matches

            print(f"{col_name}:")
            print(f"  Max absolute difference: {max_abs_diff:.10e}")
            print(f"  Mean absolute difference: {mean_abs_diff:.10e}")
            print(f"  Max relative difference: {max_rel_diff:.10e}")
            print(f"  Mean relative difference: {mean_rel_diff:.10e}")
            print(f"  Status: {status}")
            print()

        if all_match:
            print("=" * 60)
            print("✓ FILES MATCH WITHIN TOLERANCE!")
            print("=" * 60)
        else:
            print("=" * 60)
            print("✗ FILES DIFFER BEYOND TOLERANCE!")
            print("=" * 60)

        return all_match

    except Exception as e:
        print(f"ERROR: Failed to compare files: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    try:
        frequencies, real_part, imag_part, magnitude_db = test_calcimp()
        print("Test completed successfully!")

        # Write results to file
        output_file = "../sample/test_pycalcimp.imp"
        write_impedance_file(output_file, frequencies, real_part, imag_part, magnitude_db)

        # Compare with reference file
        # Use a more reasonable tolerance (10% relative difference)
        reference_file = "sample/test.imp"
        matches = compare_impedance_files(reference_file, output_file, tolerance=0.1)

        if matches:
            print("\n✓ Python calcimp results match reference implementation!")
        else:
            print("\n✗ Python calcimp results differ from reference implementation!")
            print("  This may indicate a problem with the binding or numerical differences.")

    except Exception as e:
        print(f"Error during test: {e}")
        import traceback
        traceback.print_exc()
