// Re-export from dependencies for convenience
pub use fen::types::*;
pub use square_set::square_set::*;

// Include the generated magic numbers
include!("magic_gen.rs");

/// XorShift random number generator matching the C++ implementation
#[derive(Debug, Clone)]
pub struct XorShift {
    state: u64,
}

impl XorShift {
    pub fn new(seed: u64) -> Self {
        Self { state: seed }
    }

    pub fn next(&mut self) -> u64 {
        let result = self.state.wrapping_mul(0xd989bcacc137dcd5);
        self.state ^= self.state >> 11;
        self.state ^= self.state << 31;
        self.state ^= self.state >> 18;
        result
    }
}

impl Default for XorShift {
    fn default() -> Self {
        Self::new(0xc1f651c67c62c6e0)
    }
}

/// Parallel deposit function that deposits bits from `value` into positions specified by `mask`
pub fn parallel_deposit(value: u64, mask: u64) -> u64 {
    let mut result = 0u64;
    let mut value_bit = 1u64;
    let mut mask_copy = mask;

    while mask_copy != 0 {
        let mask_bit = mask_copy & mask_copy.wrapping_neg(); // Isolate LSB
        mask_copy &= mask_copy - 1; // Remove LSB

        if value & value_bit != 0 {
            result |= mask_bit;
        }
        value_bit <<= 1;
    }

    result
}

/// Generate blocker squares for rook on given square (excluding edges)
pub fn rook_blockers(square: Square) -> SquareSet {
    let mut result = SquareSet::new();

    // Up: for (auto to = sq; rank(to) < kMaxRank - 1;) result.insert(to = step(to, 0, 1));
    let mut current = square;
    while current.rank() < 6 {
        current = Square::make_square(current.file(), current.rank() + 1);
        result.insert(current);
    }

    // Down: for (auto to = sq; rank(to) > 1;) result.insert(to = step(to, 0, -1));
    current = square;
    while current.rank() > 1 {
        current = Square::make_square(current.file(), current.rank() - 1);
        result.insert(current);
    }

    // Right: for (auto to = sq; file(to) < kMaxFile - 1;) result.insert(to = step(to, 1, 0));
    current = square;
    while current.file() < 6 {
        current = Square::make_square(current.file() + 1, current.rank());
        result.insert(current);
    }

    // Left: for (auto to = sq; file(to) > 1;) result.insert(to = step(to, -1, 0));
    current = square;
    while current.file() > 1 {
        current = Square::make_square(current.file() - 1, current.rank());
        result.insert(current);
    }

    result
}

/// Generate blocker squares for bishop on given square (excluding edges)  
pub fn bishop_blockers(square: Square) -> SquareSet {
    let mut result = SquareSet::new();

    // Up-Right: for (auto to = sq; rank(to) < kMaxRank - 1 && file(to) < kMaxFile - 1;)
    //             result.insert(to = step(to, 1, 1));
    let mut current = square;
    while current.rank() < 6 && current.file() < 6 {
        current = Square::make_square(current.file() + 1, current.rank() + 1);
        result.insert(current);
    }

    // Up-Left: for (auto to = sq; rank(to) < kMaxRank - 1 && file(to) > 1;)
    //            result.insert(to = step(to, -1, 1));
    current = square;
    while current.rank() < 6 && current.file() > 1 {
        current = Square::make_square(current.file() - 1, current.rank() + 1);
        result.insert(current);
    }

    // Down-Right: for (auto to = sq; rank(to) > 1 && file(to) < kMaxFile - 1;)
    //               result.insert(to = step(to, 1, -1));
    current = square;
    while current.rank() > 1 && current.file() < 6 {
        current = Square::make_square(current.file() + 1, current.rank() - 1);
        result.insert(current);
    }

    // Down-Left: for (auto to = sq; rank(to) > 1 && file(to) > 1;)
    //              result.insert(to = step(to, -1, -1));
    current = square;
    while current.rank() > 1 && current.file() > 1 {
        current = Square::make_square(current.file() - 1, current.rank() - 1);
        result.insert(current);
    }

    result
}

/// Compute slider blocker squares for given piece type
pub fn compute_slider_blockers(square: Square, is_bishop: bool) -> SquareSet {
    if is_bishop {
        bishop_blockers(square)
    } else {
        rook_blockers(square)
    }
}

/// Compute rook attack targets given blockers
pub fn compute_rook_targets(square: Square, blockers: SquareSet) -> SquareSet {
    let rank = square.rank();
    let file = square.file();
    let mut result = SquareSet::new();

    // Up
    for r in (rank + 1)..8 {
        let target = Square::make_square(file, r);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
    }

    // Down
    for r in (0..rank).rev() {
        let target = Square::make_square(file, r);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
    }

    // Right
    for f in (file + 1)..8 {
        let target = Square::make_square(f, rank);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
    }

    // Left
    for f in (0..file).rev() {
        let target = Square::make_square(f, rank);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
    }

    result
}

/// Compute bishop attack targets given blockers
pub fn compute_bishop_targets(square: Square, blockers: SquareSet) -> SquareSet {
    let rank = square.rank();
    let file = square.file();
    let mut result = SquareSet::new();

    // Up-Right diagonal
    let mut r = rank + 1;
    let mut f = file + 1;
    while r < 8 && f < 8 {
        let target = Square::make_square(f, r);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
        r += 1;
        f += 1;
    }

    // Up-Left diagonal
    r = rank + 1;
    f = file;
    while r < 8 && f > 0 {
        f -= 1;
        let target = Square::make_square(f, r);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
        r += 1;
    }

    // Down-Right diagonal
    r = rank;
    f = file + 1;
    while r > 0 && f < 8 {
        r -= 1;
        let target = Square::make_square(f, r);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
        f += 1;
    }

    // Down-Left diagonal
    r = rank;
    f = file;
    while r > 0 && f > 0 {
        r -= 1;
        f -= 1;
        let target = Square::make_square(f, r);
        result.insert(target);
        if blockers.contains(target) {
            break;
        }
    }

    result
}

/// Compute slider attack targets for given piece type
pub fn compute_slider_targets(square: Square, is_bishop: bool, blockers: SquareSet) -> SquareSet {
    if is_bishop {
        compute_bishop_targets(square, blockers)
    } else {
        compute_rook_targets(square, blockers)
    }
}

/// Magic bitboard structure for efficient attack lookups
#[derive(Debug, Clone)]
pub struct Magic {
    pub magic: u64,
    pub mask: SquareSet,
    pub table: Vec<SquareSet>,
    pub shift: u32,
}

impl Magic {
    /// Create a new Magic structure for the given square and piece type
    pub fn new(square: Square, is_bishop: bool, magic: u64) -> Self {
        let mask = compute_slider_blockers(square, is_bishop);
        let bits = mask.len();
        let shift = 64 - bits;
        let table_size = 1usize << bits;
        let mut table = vec![SquareSet::new(); table_size];

        // Fill the table with all possible blocker configurations
        for i in 0..(1 << bits) {
            let blockers = SquareSet::from_bits(parallel_deposit(i as u64, mask.bits()));
            let targets = compute_slider_targets(square, is_bishop, blockers);
            let index = ((blockers.bits().wrapping_mul(magic)) >> shift) as usize;

            // Verify no collisions (in a perfect magic)
            if !table[index].is_empty() {
                assert_eq!(table[index], targets, "Magic collision detected");
            } else {
                table[index] = targets;
            }
        }

        Self {
            magic,
            mask,
            table,
            shift,
        }
    }

    /// Get attack targets for given occupancy
    pub fn targets(&self, occupancy: SquareSet) -> SquareSet {
        let blockers = occupancy & self.mask;
        let index = ((blockers.bits().wrapping_mul(self.magic)) >> self.shift) as usize;
        self.table[index]
    }
}

/// Initialize magic tables for all squares
pub fn init_magics() -> (Vec<Magic>, Vec<Magic>) {
    let mut rook_magics = Vec::with_capacity(64);
    let mut bishop_magics = Vec::with_capacity(64);

    for square_idx in 0..64 {
        let square = unsafe { std::mem::transmute(square_idx as u8) };
        rook_magics.push(Magic::new(square, false, ROOK_MAGICS[square_idx]));
        bishop_magics.push(Magic::new(square, true, BISHOP_MAGICS[square_idx]));
    }

    (rook_magics, bishop_magics)
}

/// Global magic tables (initialized lazily)
static MAGIC_TABLES: std::sync::OnceLock<(Vec<Magic>, Vec<Magic>)> = std::sync::OnceLock::new();

fn get_magic_tables() -> &'static (Vec<Magic>, Vec<Magic>) {
    MAGIC_TABLES.get_or_init(init_magics)
}

/// Get attack targets for a piece on a square with given occupancy
pub fn targets(square: Square, is_bishop: bool, occupancy: SquareSet) -> SquareSet {
    let (rook_magics, bishop_magics) = get_magic_tables();
    let magics = if is_bishop {
        bishop_magics
    } else {
        rook_magics
    };
    magics[square as usize].targets(occupancy)
}

/// Generate a random magic number candidate
pub fn random_magic(mask: SquareSet, rng: &mut XorShift) -> u64 {
    loop {
        let magic = rng.next() & rng.next() & rng.next();
        let test_bits = SquareSet::from_bits(mask.bits().wrapping_mul(magic))
            & SquareSet::from_bits(0xff00_0000_0000_0000u64);
        if test_bits.len() >= 6 {
            return magic;
        }
    }
}

/// Check if a magic number is valid for the given square and piece type
pub fn check_magic(
    square: Square,
    is_bishop: bool,
    magic: u64,
    targets_list: &[SquareSet],
    blockers_list: &[SquareSet],
) -> bool {
    let mask = compute_slider_blockers(square, is_bishop);
    let bits = mask.len();
    let shift = 64 - bits;
    let table_size = 1usize << bits;
    let mut table = vec![None; table_size];

    for i in 0..(1 << bits) {
        let index = ((blockers_list[i].bits().wrapping_mul(magic)) >> shift) as usize;
        if index >= table_size {
            return false;
        }

        match &table[index] {
            None => table[index] = Some(targets_list[i]),
            Some(existing) => {
                if *existing != targets_list[i] {
                    return false; // Collision
                }
            }
        }
    }

    true
}

/// Find a magic number for the given square and piece type
/// Returns (magic_number, attempts_made) if successful, or None if failed
pub fn find_magic(square: Square, is_bishop: bool, rng: &mut XorShift) -> Option<(u64, usize)> {
    let mask = compute_slider_blockers(square, is_bishop);
    let bits = mask.len();

    // Precompute all blocker configurations and their targets
    let mut blockers_list = Vec::new();
    let mut targets_list = Vec::new();

    for i in 0..(1 << bits) {
        let blockers = SquareSet::from_bits(parallel_deposit(i as u64, mask.bits()));
        let targets = compute_slider_targets(square, is_bishop, blockers);
        blockers_list.push(blockers);
        targets_list.push(targets);
    }

    // Try to find a magic number
    const MAX_TRIES: usize = 1_000_000;
    for attempts in 1..=MAX_TRIES {
        let magic = random_magic(mask, rng);
        if check_magic(square, is_bishop, magic, &targets_list, &blockers_list) {
            return Some((magic, attempts));
        }
    }

    None
}

/// Find a magic number for the given square and piece type (legacy interface)
/// This is kept for backward compatibility with existing tests
pub fn find_magic_simple(square: Square, is_bishop: bool, rng: &mut XorShift) -> Option<u64> {
    find_magic(square, is_bishop, rng).map(|(magic, _)| magic)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parallel_deposit() {
        // Test simple case
        let value = 0b101;
        let mask = 0b1001001;
        let result = parallel_deposit(value, mask);
        assert_eq!(result, 0b1000001);
    }

    #[test]
    fn test_blocker_generation() {
        // Test rook blockers for center square
        let blockers = rook_blockers(Square::E4);
        assert!(blockers.contains(Square::E2));
        assert!(blockers.contains(Square::E6));
        assert!(blockers.contains(Square::B4));
        assert!(blockers.contains(Square::G4));
        // Should not contain edges
        assert!(!blockers.contains(Square::E1));
        assert!(!blockers.contains(Square::E8));
        assert!(!blockers.contains(Square::A4));
        assert!(!blockers.contains(Square::H4));

        // Test bishop blockers for center square
        let blockers = bishop_blockers(Square::E4);
        assert!(blockers.contains(Square::D3));
        assert!(blockers.contains(Square::F5));
        assert!(blockers.contains(Square::C2));
        assert!(blockers.contains(Square::G6));
        // Should not contain edges
        assert!(!blockers.contains(Square::A8));
        assert!(!blockers.contains(Square::H1));
    }

    #[test]
    fn test_attack_generation() {
        // Test rook attacks with no blockers
        let attacks = compute_rook_targets(Square::E4, SquareSet::new());
        assert_eq!(attacks.len(), 14); // 7 horizontal + 7 vertical
        assert!(attacks.contains(Square::E1));
        assert!(attacks.contains(Square::E8));
        assert!(attacks.contains(Square::A4));
        assert!(attacks.contains(Square::H4));

        // Test rook attacks with blocker
        let blocker = SquareSet::from_square(Square::E6);
        let attacks = compute_rook_targets(Square::E4, blocker);
        assert!(attacks.contains(Square::E5));
        assert!(attacks.contains(Square::E6)); // Include the blocker
        assert!(!attacks.contains(Square::E7)); // But not beyond it
    }

    #[test]
    fn test_magic_lookup() {
        // Test that magic lookup works
        let occupancy = SquareSet::from_square(Square::E6);
        let rook_attacks = targets(Square::E4, false, occupancy);
        let bishop_attacks = targets(Square::E4, true, occupancy);

        // Rook should attack along files and ranks
        assert!(rook_attacks.contains(Square::E1));
        assert!(rook_attacks.contains(Square::A4));

        // Bishop should attack along diagonals
        assert!(bishop_attacks.contains(Square::A8));
        assert!(bishop_attacks.contains(Square::H1));
    }
}
