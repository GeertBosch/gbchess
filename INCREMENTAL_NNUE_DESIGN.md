# Incremental NNUE Integration Design

## Overview

This document outlines a practical approach to add incremental NNUE evaluation to the chess engine,
targeting the realistic speedup of ~1.2x identified in our corrected analysis.

**Important Update**: Based on expert feedback, only the input transform (feature→accumulator) can
be updated incrementally, not the affine layers. The expected speedup is modest (~1.2x), not the
initially estimated 11x.

## Key Design Principles

1. **Minimal Disruption**: Keep existing evaluation interfaces unchanged
2. **RAII Safety**: Automatic state management to prevent bugs
3. **Gradual Implementation**: Can be developed and tested incrementally
4. **Performance Focused**: Only add overhead when beneficial

## Implementation Strategy

### Phase 1: Basic Infrastructure

1. **Add EvaluationContext Class**
   ```cpp
   class EvaluationContext {
       Accumulator accumulator;
       bool cacheValid = false;
       const NNUE& network;
   };
   ```

2. **Modify SearchContext**
   - Add optional `EvaluationContext` member
   - Initialize at search root
   - Thread-local storage for multi-threaded search

3. **Add Incremental Evaluation Option**
   ```cpp
   constexpr bool incrementalNNUE = false;  // Default off during development
   ```

### Phase 2: Feature Extraction

1. **Implement HalfKP Feature Extraction**
   ```cpp
   void computeFeatureChangesFromMove(Move move, const BoardChange& change,
                                      std::vector<size_t>& removedFeatures,
                                      std::vector<size_t>& addedFeatures);
   ```

2. **Integrate with Move Making**
   - Modify `makeMoveWithEval` to use incremental context when available
   - Add RAII `MoveScope` for automatic state management
   - Use efficient `BoardChange` (6 bytes) instead of copying full `Position` structs (64+ bytes)

### Phase 3: Incremental Updates

1. **Accumulator Updates**
   ```cpp
   void updateAccumulator(const std::vector<size_t>& removed, 
                         const std::vector<size_t>& added);
   ```

**Note**: Only accumulator updates are possible incrementally. All affine layers (512→32, 32→32,
32→1) require full dense matrix computation and cannot be updated incrementally.

### Phase 4: Integration Points

1. **Modify makeMoveWithEval()**
   ```cpp
   if constexpr (options::incrementalNNUE) {
       if (auto& ctx = searchContext.nnueContext) {
           auto moveScope = ctx->applyMove(move, boardChange);
           eval = moveScope.evaluateAsScore();
           return {undo, eval};
       }
   }
   // Fallback to existing evaluation...
   ```

2. **Add Performance Monitoring**
   - Track incremental vs non-incremental usage
   - Measure actual speedup achieved
   - Add statistics to `nnue_stats.cpp`

## Expected Performance Impact

Based on our corrected analysis:
- **Only input transform can be incremental**: ~18-43% of evaluation time (varies by position
  complexity)
- **Affine layers require full computation**: Dense matrix operations cannot be updated
  incrementally
- **Realistic speedup potential**: ~1.2-2.0x in evaluation-heavy positions
- **Overall search impact**: Modest improvement, not dramatic

**Key Limitation**: Unlike initial estimates, the 512→32 affine layer cannot be updated
incrementally because each of the 32 outputs depends on all 512 inputs with no locality.

## Implementation Challenges

1. **Feature Extraction Complexity**: HalfKP features are non-trivial to compute
2. **Memory Management**: Accumulator state needs careful management
3. **Debugging**: Incremental bugs can be subtle and hard to track
4. **Thread Safety**: Multiple search threads need isolated contexts

## Testing Strategy

1. **Correctness First**: Ensure incremental == non-incremental results
2. **Performance Validation**: Measure actual speedup in real games
3. **Regression Testing**: Verify no search quality degradation
4. **Unit Tests**: Test incremental updates with known positions

## Future Enhancements

1. **Lazy Evaluation**: Only compute NNUE when needed
2. **SIMD Optimization**: Vectorize accumulator updates  
3. **Larger Networks**: Enable efficient evaluation of bigger NNUE models
4. **Alternative Architectures**: Explore network designs with more incremental-friendly structures

**Note**: Multi-level caching of affine layers is not feasible due to the dense matrix structure of
NNUE networks.

## Interface Design

The incremental NNUE interface is designed for maximum efficiency:

1. **Compact Change Representation**: Uses `BoardChange` (6 bytes) + `Move` (~4 bytes) instead of full position copies
2. **Direct Feature Computation**: Computes feature changes directly from move data rather than extracting all features from positions
3. **RAII Safety**: Automatic state restoration with `MoveScope` destructor
4. **Single Efficient Interface**: No confusing "legacy" vs "preferred" options - only the efficient path is available

```cpp
// Efficient interface (only option available)
auto moveScope = context.applyMove(move, boardChange);
int32_t eval = moveScope.evaluate();
```

## Risk Mitigation

- Keep `incrementalNNUE = false` by default until thoroughly tested
- Maintain fallback to non-incremental evaluation
- Add extensive validation during development
- Monitor for any search quality regressions

This design provides a clear path to implement incremental NNUE evaluation while minimizing risk.
While the performance improvements are more modest than initially estimated (~1.2x rather than 11x),
incremental evaluation can still provide meaningful benefits in evaluation-heavy search scenarios.
