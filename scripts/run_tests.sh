#!/bin/bash
# scripts/run_tests.sh - Comprehensive test runner

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║        Test Runner - Ticketing System        ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════╝${NC}"
echo ""

# Parse arguments
RUN_UNIT=true
RUN_INTEGRATION=true
USE_DOCKER=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --unit-only)
            RUN_INTEGRATION=false
            shift
            ;;
        --integration-only)
            RUN_UNIT=false
            shift
            ;;
        --docker)
            USE_DOCKER=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--unit-only] [--integration-only] [--docker]"
            exit 1
            ;;
    esac
done

# Check if build exists
if [ ! -d "build" ] && [ "$RUN_UNIT" = true ]; then
    echo -e "${YELLOW}Build directory not found. Building project...${NC}"
    ./scripts/build.sh
fi

# ============================================================================
# UNIT TESTS
# ============================================================================
if [ "$RUN_UNIT" = true ]; then
    echo -e "${BLUE}[1/2] Running Unit Tests...${NC}"
    echo ""
    
    if [ -f "build/test_ticket" ]; then
        cd build
        
        # Run unit tests
        if ./test_ticket --gtest_color=yes; then
            echo ""
            echo -e "${GREEN}✓ Unit tests passed${NC}"
        else
            echo ""
            echo -e "${RED}✗ Unit tests failed${NC}"
            cd ..
            exit 1
        fi
        
        cd ..
    else
        echo -e "${RED}✗ Unit test executable not found${NC}"
        echo "Make sure to build with: ./scripts/build.sh"
        exit 1
    fi
    
    echo ""
fi

# ============================================================================
# INTEGRATION TESTS
# ============================================================================
if [ "$RUN_INTEGRATION" = true ]; then
    echo -e "${BLUE}[2/2] Running Integration Tests...${NC}"
    echo ""
    
    # Check if we should use Docker or native
    if [ "$USE_DOCKER" = true ]; then
        echo "Starting services with Docker..."
        
        # Start services in background
        docker-compose up -d
        
        # Wait for services
        echo "Waiting for services to be ready..."
        sleep 10
        
        # Run integration tests
        if [ -f "tests/integration/integration_test.sh" ]; then
            bash tests/integration/integration_test.sh
            TEST_RESULT=$?
            
            # Cleanup
            echo ""
            echo "Stopping Docker services..."
            docker-compose down
            
            if [ $TEST_RESULT -eq 0 ]; then
                echo -e "${GREEN}✓ Integration tests passed${NC}"
            else
                echo -e "${RED}✗ Integration tests failed${NC}"
                exit 1
            fi
        else
            echo -e "${RED}✗ Integration test script not found${NC}"
            docker-compose down
            exit 1
        fi
    else
        echo "Checking if services are running..."
        
        # Check if Back-Office is running
        if curl -s -f http://localhost:8080/health > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Back-Office is running${NC}"
            
            # Run integration tests
            if [ -f "tests/integration/integration_test.sh" ]; then
                bash tests/integration/integration_test.sh
                TEST_RESULT=$?
                
                if [ $TEST_RESULT -eq 0 ]; then
                    echo -e "${GREEN}✓ Integration tests passed${NC}"
                else
                    echo -e "${RED}✗ Integration tests failed${NC}"
                    exit 1
                fi
            else
                echo -e "${RED}✗ Integration test script not found${NC}"
                exit 1
            fi
        else
            echo -e "${RED}✗ Back-Office is not running${NC}"
            echo ""
            echo "Please start services first:"
            echo "  Option 1 (Docker): docker-compose up -d"
            echo "  Option 2 (Manual): ./build/bin/backoffice &"
            echo ""
            echo "Or run with Docker: $0 --docker"
            exit 1
        fi
    fi
fi

# ============================================================================
# SUCCESS
# ============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          ALL TESTS PASSED! ✓✓✓                ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
echo ""
