clear

echo -e "\033[1m🔨 Building project...\033[0m" && \
idf.py -DROBOTICK_WORKLOADS_USE_ALL=ON build
