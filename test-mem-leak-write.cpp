#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <unistd.h> // For getpid() - standard on POSIX systems
#include <iomanip> // **FIX: Required for std::setprecision and std::fixed**

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

// --- Configuration Constants ---

// The amount of data read by read_file (31 MB)
constexpr size_t WRITE_SIZE =  30 * 1024 * 1024;
// Number of iterations in the main loop
constexpr int NUM_ITERATIONS = 50;

/**
 * @brief Helper function to return the current process Resident Set Size (RSS) in MB.
 *
 * This implementation is non-portable and targets Linux/POSIX environments
 * by reading the /proc/self/status file.
 * @return Current RSS in MB, or 0.0 if unable to read.
 */

 long get_current_rss_mb() {
  // Default is getting memory usage for self (calling process)
  std::string path = "/proc/self/stat";
  std::ifstream stat_stream(path, std::ios_base::in);

  double resident_set = 0.0;
  // Temporary variables for irrelevant leading entries in stats
  std::string temp_pid, comm, state, ppid, pgrp, session, tty_nr;
  std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
  std::string utime, stime, cutime, cstime, priority, nice;
  std::string O, itrealvalue, starttime, vsize;

  // Get rss to find memory usage
  long rss;
  stat_stream >> temp_pid >> comm >> state >> ppid >> pgrp >> session >>
      tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >>
      utime >> stime >> cutime >> cstime >> priority >> nice >> O >>
      itrealvalue >> starttime >> vsize >> rss;
  stat_stream.close();

  // pid does not connect to an existing process
  assert(!state.empty());

  // Calculations in case x86-64 is configured to use 2MB pages
  long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
  resident_set = rss * page_size_kb;
  // Memory in KB
  return resident_set / 1024;
}


/**
 * @brief Writes a large chunk of data to the file from a temporary buffer.
 *
 * The buffer (data_buffer) is allocated on the stack/heap inside this function
 * and should be automatically released upon function exit.
 * @param file_path The path to the file to write.
 */
void write_file(const std::string& file_path) {
    // Open the file for writing in binary mode, truncating if it exists
    std::ofstream file(file_path, std::ios::binary | std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        std::cerr << "Error: File '" << file_path << "' not found." << std::endl;
        return;
    }

    // Allocate a buffer on the heap using std::vector. This simulates the
    // temporary memory consumption when reading the large file chunk.
    // In Python, this is what f.read(WRITE_SIZE) does internally.
    std::vector<char> data_buffer(WRITE_SIZE);

    // Write the data from the buffer.
    file.write(data_buffer.data(), WRITE_SIZE);

    file.close();

    // The data_buffer (and its underlying memory) is automatically released
    // when the function exits (goes out of scope).
}


/**
 * @brief Simulates a task that involves repeated memory allocation and resource creation.
 * @param file_path The path to the file used for reading.
 */
void trigger_mem() {
    double initial_rss = get_current_rss_mb();
    std::cout << "PID: " << getpid() << std::endl;
    std::cout << "Initial RSS: " << initial_rss << " MB" << std::endl;
    std::cout << "---------------------------------------------------------" << std::endl;

    std::string file_path = "/tmp/test_file.txt";

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // --- 1. File Read Simulation (Memory Spike) ---
        // Delete the file before writing
        if (std::remove(file_path.c_str()) != 0) {
            // perror("remove"); // Optional: Okay if file doesn't exist
        }
        // Offload the file writing to a separate thread
        std::thread t(write_file, file_path);
        
        // Wait for the thread to finish. This ensures the memory allocated
        // inside 'write_file' is released before the next iteration (unless a leak occurs).
        t.join();

        // --- 2. Resource Creation/Closing Simulation ---
        // Create and immediately close a real gRPC channel.
        std::string address = "localhost:" + std::to_string(4000 + i);
        auto creds = grpc::InsecureChannelCredentials();
        auto channel = grpc::CreateChannel(address, creds);
        // channel goes out of scope here, and its destructor will handle cleanup.

        // --- 3. Monitoring and Output ---
        double current_rss = get_current_rss_mb();
        double diff_from_start = current_rss - initial_rss;

        std::cout << "Iteration " << i + 1 << "/" << NUM_ITERATIONS << ": "
                  << "Current RSS: " << std::fixed << std::setprecision(2) << current_rss << " MB | "
                  << "Total increase: +" << std::fixed << std::setprecision(2) << diff_from_start << " MB"
                  << std::endl;
        
        // Sleep to mimic real-world processing pause
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


int main(int argc, char* argv[]) {
    // C++ doesn't use the Python logging module, but we can set up formatting
    // for standard output (std::fixed and std::setprecision are for memory printing).
    std::cout << std::fixed << std::setprecision(2);

    // --- Run the Memory Trigger Simulation ---
    trigger_mem();

    return 0;
}