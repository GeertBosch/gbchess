mod elo;

use elo::{ELO, Result};

fn test_basic_elo() {
    let player = ELO::new();
    assert_eq!(player.rating(), ELO::INITIAL_RATING);
    println!("Initial player rating: {}", player.rating());

    // Simulate a win against an opponent rated 400 points higher
    let opponent = ELO::with_rating(ELO::INITIAL_RATING + 400);
    assert_eq!(opponent.rating(), ELO::INITIAL_RATING + 400);
    
    let mut player_after_win = player;
    player_after_win.update_one(opponent, Result::Win);
    println!("After win over opponent rated {}: {}", opponent.rating(), player_after_win.rating());
    assert!(player_after_win.rating() > player.rating());
    assert!(player_after_win.rating() < opponent.rating());

    // Simulate both a win and a loss over the stronger opponent
    let mut player_after_win_and_loss = player_after_win;
    player_after_win_and_loss.update_one(opponent, Result::Loss);
    println!("After subsequent loss against this opponent: {}", player_after_win_and_loss.rating());
    assert!(player_after_win_and_loss.rating() > player.rating()); // Should still be higher than initial
    assert!(player_after_win_and_loss.rating() < player_after_win.rating()); // Should be lower than after the win
}

fn test_win_25_percent_against_one() {
    const NUM_ROUNDS: i32 = 100;
    const OPPONENT_DIFF: i32 = 400;
    const OPPONENT_RATING: i32 = ELO::INITIAL_RATING + OPPONENT_DIFF;
    
    let mut player = ELO::new();
    let mut opponent = ELO::with_rating(OPPONENT_RATING); // Same opponent every time
    
    // Simulate a player winning 25% of the time against a given opponent rated 400 points higher
    for round in 0..NUM_ROUNDS {
        let result = if round % 4 == 0 { Result::Win } else { Result::Loss };
        player.update_both(&mut opponent, result);
    }

    println!("After winning 25% of {} rounds against a {} rated opponent: {} vs {}", 
             NUM_ROUNDS, OPPONENT_RATING, player.rating(), opponent.rating());
    
    // No ELO points should get lost
    assert_eq!(player.rating() + opponent.rating(), ELO::INITIAL_RATING + OPPONENT_RATING);

    // At a 25% win rate, expect to be 200 points below the opponent
    const EXPECTED_DIFF: i32 = -200;
    let diff = player.rating() - opponent.rating();
    assert!(diff > EXPECTED_DIFF - ELO::K);
    assert!(diff < EXPECTED_DIFF + ELO::K);
}

fn test_win_draw_lose_draw_against_one() {
    const NUM_ROUNDS: i32 = 100;
    const OPPONENT_DIFF: i32 = -100;
    const OPPONENT_RATING: i32 = ELO::INITIAL_RATING + OPPONENT_DIFF;
    
    let mut player = ELO::new();
    let mut opponent = ELO::with_rating(OPPONENT_RATING);
    
    // After a player sequentially wins/draws/loses/draws many times against an opponent 
    // rated 100 points lower, their rating should converge toward their average
    for round in 0..NUM_ROUNDS {
        let result = match round % 4 {
            0 => Result::Win,
            1 => Result::Draw,
            2 => Result::Loss,
            3 => Result::Draw,
            _ => unreachable!(),
        };
        player.update_both(&mut opponent, result);
    }

    println!("After win/draw/lose/draw {} rounds against {} rated opponents: {} vs {}", 
             NUM_ROUNDS, OPPONENT_RATING, player.rating(), opponent.rating());
    assert_eq!(player.rating() + opponent.rating(), ELO::INITIAL_RATING + OPPONENT_RATING);
    assert!(player.rating() < ELO::INITIAL_RATING); // Lost some
    let diff = player.rating() - opponent.rating();
    assert!(diff > -ELO::K);
    assert!(diff < ELO::K);
}

fn test_win_50_percent_against_many() {
    const NUM_ROUNDS: i32 = 100;
    const OPPONENT_RATING: i32 = ELO::INITIAL_RATING + 400;
    
    let mut player = ELO::new();
    
    // After a player wins half of the time against opponents rated 400 points higher,
    // their rating should converge toward the opponent rating
    for round in 0..NUM_ROUNDS {
        let opponent = ELO::with_rating(OPPONENT_RATING); // New opponent every time
        let result = if round % 2 == 0 { Result::Win } else { Result::Loss };
        player.update_one(opponent, result);
    }

    println!("After winning 50% of {} rounds against {} rated opponents: {}", 
             NUM_ROUNDS, OPPONENT_RATING, player.rating());
    assert!(player.rating() > ELO::INITIAL_RATING);
    assert!(player.rating() > OPPONENT_RATING - ELO::K);
    assert!(player.rating() < OPPONENT_RATING + ELO::K);
}

fn test_win_90_percent_against_many() {
    const NUM_ROUNDS: i32 = 250;
    const OPPONENT_RATING: i32 = 1600;
    
    let mut player = ELO::new();
    
    // Simulate a player who wins 90% of the time against opponents at a 1600 rating
    for round in 0..NUM_ROUNDS {
        let opponent = ELO::with_rating(OPPONENT_RATING); // New opponent every time
        let result = if round % 10 == 0 { Result::Loss } else { Result::Win };
        player.update_one(opponent, result);
    }

    println!("After winning 90% of {} rounds against {} rated opponents: {}", 
             NUM_ROUNDS, OPPONENT_RATING, player.rating());

    const EXPECTED_DIFF: i32 = 400;
    let diff = player.rating() - OPPONENT_RATING;
    assert!(diff > EXPECTED_DIFF - ELO::K);
    assert!(diff < EXPECTED_DIFF + ELO::K);
}

fn main() {
    test_basic_elo();
    test_win_50_percent_against_many();
    test_win_25_percent_against_one();
    test_win_90_percent_against_many();
    test_win_draw_lose_draw_against_one();
    println!("All ELO tests passed.");
}
