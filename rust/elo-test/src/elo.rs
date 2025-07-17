use std::f64;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Result {
    Loss = 0,
    Draw = 1,
    Win = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ELO {
    rating: i32,
}

impl ELO {
    pub const INITIAL_RATING: i32 = 800;
    pub const MIN_RATING: i32 = 100;
    pub const MAX_RATING: i32 = 4000;
    pub const K: i32 = 32; // Maximum rating change per game

    /// Create a new ELO with the initial rating
    pub fn new() -> Self {
        Self {
            rating: Self::INITIAL_RATING,
        }
    }

    /// Create a new ELO with a specific rating, clamped to valid range
    pub fn with_rating(rating: i32) -> Self {
        Self {
            rating: rating.clamp(Self::MIN_RATING, Self::MAX_RATING),
        }
    }

    /// Get the current rating
    pub fn rating(&self) -> i32 {
        self.rating
    }

    /// Update this player's rating after a game against an opponent
    /// The opponent's rating is not modified (one-way update)
    pub fn update_one(&mut self, opponent: ELO, result: Result) {
        let mut temp_opponent = opponent;
        self.update_both(&mut temp_opponent, result);
    }

    /// Update both players' ratings after a game
    /// Transfers rating points between the two players
    pub fn update_both(&mut self, opponent: &mut ELO, result: Result) {
        let rating_diff = opponent.rating - self.rating;
        let expected = 1.0 / (1.0 + f64::powf(10.0, rating_diff as f64 / 400.0));
        let change = (Self::K as f64 * (result as i32 as f64 * 0.5 - expected)).round() as i32;
        
        self.rating += change;
        opponent.rating -= change;
    }
}

impl Default for ELO {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_elo_has_initial_rating() {
        let elo = ELO::new();
        assert_eq!(elo.rating(), ELO::INITIAL_RATING);
    }

    #[test]
    fn test_with_rating_clamps_values() {
        let low = ELO::with_rating(ELO::MIN_RATING - 100);
        assert_eq!(low.rating(), ELO::MIN_RATING);

        let high = ELO::with_rating(ELO::MAX_RATING + 100);
        assert_eq!(high.rating(), ELO::MAX_RATING);

        let normal = ELO::with_rating(1500);
        assert_eq!(normal.rating(), 1500);
    }

    #[test]
    fn test_rating_conservation() {
        let mut player1 = ELO::with_rating(1000);
        let mut player2 = ELO::with_rating(1200);
        let initial_sum = player1.rating() + player2.rating();

        player1.update_both(&mut player2, Result::Win);
        let final_sum = player1.rating() + player2.rating();
        
        assert_eq!(initial_sum, final_sum, "Rating points should be conserved");
    }

    #[test]
    fn test_draw_has_no_effect_on_equal_players() {
        let mut player1 = ELO::with_rating(1200);
        let mut player2 = ELO::with_rating(1200);
        let initial_rating = player1.rating();

        player1.update_both(&mut player2, Result::Draw);
        
        assert_eq!(player1.rating(), initial_rating);
        assert_eq!(player2.rating(), initial_rating);
    }
}
