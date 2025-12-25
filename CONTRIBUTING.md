# Contributing

## Code Style

### C++
- C++20 standard
- 4-space indentation
- `snake_case` for functions and variables
- `PascalCase` for types
- `SCREAMING_SNAKE_CASE` for constants
- No exceptions on hot path
- Prefer `constexpr` where possible

### Comments
- Document non-obvious design decisions
- Explain "why" not "what"
- Use `// TODO:` for future work

### Commits
- Descriptive commit messages
- Reference issues where applicable
- Keep commits atomic

## Building

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### With Tests
```bash
cmake -DBUILD_TESTS=ON ..
```

## Testing

```bash
./bin/unit_tests
```

## Benchmarking

```bash
./scripts/benchmark.sh
```

## Code Review Checklist

- [ ] No heap allocation on hot path
- [ ] Cache-line alignment for shared data
- [ ] Lock-free where applicable
- [ ] Unit tests for new functionality
- [ ] Benchmark impact documented
