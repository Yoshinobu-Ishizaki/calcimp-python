#!/usr/bin/env python3
"""
Test script for calcimp Python module
Reads sample/test.men and calculates input impedance
"""

import numpy as np
import calcimp

def test_calcimp():
    # Path to the test mensur file
    mensur_file = "sample/test.men"

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

if __name__ == "__main__":
    try:
        frequencies, real_part, imag_part, magnitude_db = test_calcimp()
        print("Test completed successfully!")
    except Exception as e:
        print(f"Error during test: {e}")
        import traceback
        traceback.print_exc()
