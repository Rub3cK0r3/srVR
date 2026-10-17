#!/bin/bash
# test_server.sh - Quick test script for srVR with epoll enabled

echo "Building srVR server..."
make clean
make

echo ""
echo "Testing server with epoll enabled..."
echo "Creating config with use_epoll=1..."

cat > config/server.conf << EOF
port=8080
document_root=./www
use_epoll=1
EOF

echo ""
echo "Starting server in background..."
timeout 10 ./bin/srVR &
SERVER_PID=$!
SERVER_PORT=8080

# Give server time to start
sleep 1

echo ""
echo "Testing HTTP requests..."
echo ""

# Test 1: GET request
echo "Test 1: GET /index.html"
curl -v http://localhost:${SERVER_PORT}/index.html 2>&1 | head -20
echo ""

# Test 2: HEAD request  
echo "Test 2: HEAD request"
curl -I http://localhost:${SERVER_PORT}/ 2>&1
echo ""

# Test 3: 404 error
echo "Test 3: 404 Not Found"
curl -s http://localhost:${SERVER_PORT}/nonexistent.html | head -5
echo ""

# Test 4: Directory traversal protection
echo "Test 4: Directory Traversal Protection"
curl -s http://localhost:${SERVER_PORT}/../etc/passwd | head -3
echo ""

# Test 5: POST request
echo "Test 5: POST request"
curl -X POST -d "test data" http://localhost:${SERVER_PORT}/ 2>&1 | head -10
echo ""

# Clean up
wait $SERVER_PID 2>/dev/null
echo ""
echo "All tests completed."
echo "Check server.log for detailed logs:"
echo "  tail -20 server.log"
