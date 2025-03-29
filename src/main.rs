use std::thread;
use std::time::Instant;
use std::hint::black_box;
extern crate num_cpus; // Ensure num_cpus is added as a dependency

fn format_large_number(number: u64) -> String {
    if number >= 1_000_000_000 {
        format!("{:.1} billion", number as f64 / 1_000_000_000.0)
    } else if number >= 1_000_000 {
        format!("{:.1} million", number as f64 / 1_000_000.0)
    } else if number >= 1_000 {
        format!("{:.1} thousand", number as f64 / 1_000.0)
    } else {
        format!("{}", number)
    }
}

// Flytta loopen till en separat funktion
fn loop_work() {
    for i in 0u32..=u32::MAX {
        black_box(i); // Förhindra optimering
    }
}

fn speed_bench(thread_count: usize, label: &str) {
    let total_iterations = (u32::MAX as u64 + 1) * thread_count as u64; // Beräkna totala iterationer
    println!("\n[{}] Starting benchmark with {} thread(s): Counting from 0 to {}.", 
             label, thread_count, total_iterations - 1); // Justera utskriften
    println!("{} is {}", total_iterations - 1, format_large_number(total_iterations - 1)); // Lägg till denna rad
    let start = Instant::now();
    let mut handles = Vec::new();
    for _ in 0..thread_count {
        handles.push(thread::spawn(|| {
            loop_work(); // Anropa den separata loop-funktionen
        }));
    }
    for handle in handles {
        handle.join().unwrap();
    }
    let duration = start.elapsed();
    println!("[{}] Benchmark finished. Time elapsed: {:?}", label, duration);
}

fn main() {
    println!("Rust speed benchmark starting.");
    let logical = num_cpus::get();
    let physical = num_cpus::get_physical();
    println!("Detected {} logical cores and {} physical cores.", logical, physical);
    
    if logical > physical {
        speed_bench(physical, "Physical Cores Only");
        speed_bench(logical, "Logical + Physical Cores");
    } else {
        speed_bench(logical, "Benchmark");
    }
}