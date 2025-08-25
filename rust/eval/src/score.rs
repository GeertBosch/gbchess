use std::fmt;
use std::ops::{Add, AddAssign, Mul, MulAssign, Neg, Sub, SubAssign};

/// The Score type represents the evaluation of a chess position in centipawns.
/// Positive scores indicate an advantage for white, negative for black.
/// The value range is -9999 to 9999, with the extremes indicating checkmate.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Default)]
pub struct Score {
    value: i16, // centipawns
}

impl Score {
    const MAX_VALUE: i16 = 9999;
    const MIN_VALUE: i16 = -9999;

    /// Create a Score from centipawn value with bounds checking
    pub const fn from_cp(cp: i16) -> Self {
        assert!(cp >= Self::MIN_VALUE && cp <= Self::MAX_VALUE);
        Self { value: cp }
    }

    /// Create a Score representing checkmate in the given number of moves
    pub fn mate_in(moves: i16) -> Self {
        if moves < 0 {
            return -Self::mate_in(-moves);
        }
        debug_assert!(moves > 0 && moves < 100, "Invalid mate moves: {}", moves);
        Self::from_cp(Self::MAX_VALUE - moves + 1)
    }

    /// Maximum possible score (mate in 1)
    pub const fn max() -> Self {
        Self::from_cp(Self::MAX_VALUE)
    }

    /// Minimum possible score (mated in 1)
    pub const fn min() -> Self {
        Self::from_cp(Self::MIN_VALUE)
    }

    /// Draw score (0)
    pub const fn draw() -> Self {
        Self::from_cp(0)
    }

    /// Get the centipawn value
    pub fn cp(self) -> i16 {
        self.value
    }

    /// Get the value in pawns (dividing by 100)
    pub fn pawns(self) -> i16 {
        self.value / 100
    }

    /// Check if this is a mate score and return moves to mate (0 if not mate)
    pub fn mate(self) -> i16 {
        if self.value < 0 {
            return -(-self).mate();
        }
        if self.value < Self::MAX_VALUE / 100 * 100 {
            0
        } else {
            100 - self.value % 100
        }
    }
}

// Arithmetic operations
impl Add for Score {
    type Output = Self;

    fn add(self, rhs: Self) -> Self {
        Self::from_cp(self.value + rhs.value)
    }
}

impl Sub for Score {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self {
        Self::from_cp(self.value - rhs.value)
    }
}

impl Mul for Score {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        Self::from_cp(self.value * rhs.value / 100)
    }
}

impl AddAssign for Score {
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl SubAssign for Score {
    fn sub_assign(&mut self, rhs: Self) {
        *self = *self - rhs;
    }
}

impl MulAssign for Score {
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

impl Neg for Score {
    type Output = Self;

    fn neg(self) -> Self {
        Self::from_cp(-self.value)
    }
}

impl fmt::Display for Score {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.value < 0 {
            return write!(f, "-{}", -(*self));
        }

        let mate_moves = self.mate();
        if mate_moves > 0 {
            write!(f, "M{}", mate_moves)
        } else {
            let pawns = self.value / 100;
            let cents = self.value % 100;
            write!(f, "{}.{:02}", pawns, cents)
        }
    }
}

/// Create a Score from a literal centipawn value
/// This macro mimics the C++ _cp literal suffix
#[macro_export]
macro_rules! cp {
    ($value:literal) => {
        Score::from_cp($value)
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_score_creation() {
        let score = Score::from_cp(100);
        assert_eq!(score.cp(), 100);
        assert_eq!(score.pawns(), 1);
    }

    #[test]
    fn test_score_arithmetic() {
        let a = Score::from_cp(100);
        let b = Score::from_cp(50);

        assert_eq!((a + b).cp(), 150);
        assert_eq!((a - b).cp(), 50);
        assert_eq!((a * b).cp(), 50); // 100 * 50 / 100 = 50
    }

    #[test]
    fn test_score_negation() {
        let score = Score::from_cp(123);
        assert_eq!((-score).cp(), -123);
    }

    #[test]
    fn test_mate_score() {
        let mate1 = Score::mate_in(1);
        assert_eq!(mate1.mate(), 1);
        assert_eq!(mate1, Score::max());

        let mated1 = -mate1;
        assert_eq!(mated1.mate(), -1);
        assert_eq!(mated1, Score::min());
    }

    #[test]
    fn test_score_display() {
        assert_eq!(format!("{}", Score::from_cp(123)), "1.23");
        assert_eq!(format!("{}", Score::from_cp(-123)), "-1.23");
        assert_eq!(format!("{}", Score::from_cp(900)), "9.00");
        assert_eq!(format!("{}", Score::mate_in(1)), "M1");
        assert_eq!(format!("{}", -Score::mate_in(1)), "-M1");
    }

    #[test]
    fn test_cp_macro() {
        let score = cp!(150);
        assert_eq!(score.cp(), 150);
    }
}
