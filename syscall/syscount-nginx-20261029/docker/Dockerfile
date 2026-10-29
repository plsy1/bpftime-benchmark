# The ssl-nginx runtime image (nginx, wrk, python3) plus what the official
# benchmark/syscount-nginx/benchmark.py needs: sudo (it starts the kernel
# syscount with sudo) and matplotlib/numpy (imported at the top).
FROM bpftime-ssl-nginx:20261023
RUN apt-get update && apt-get install -y --no-install-recommends \
        sudo python3-matplotlib python3-numpy \
    && rm -rf /var/lib/apt/lists/*
CMD ["sleep", "infinity"]
