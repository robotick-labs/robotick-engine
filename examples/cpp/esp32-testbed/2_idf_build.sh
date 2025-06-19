clear

export WORKLOAD_PRESET=esp32-s3-workloads-all
export ROBOTICK_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.."

echo -e "\033[1m🔨 Building project...\033[0m" && \
idf.py build
