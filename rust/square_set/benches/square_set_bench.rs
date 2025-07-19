use criterion::{black_box, criterion_group, criterion_main, Criterion};
use square_set::square_set::*;

fn benchmark_square_set_operations(c: &mut Criterion) {
    let mut group = c.benchmark_group("square_set");
    
    group.bench_function("create_and_insert", |b| {
        b.iter(|| {
            let mut set = SquareSet::new();
            for i in 0..64 {
                let square = unsafe { std::mem::transmute(i as u8) };
                set.insert(black_box(square));
            }
            set
        })
    });
    
    group.bench_function("rank_file_operations", |b| {
        b.iter(|| {
            let rank = SquareSet::rank(black_box(4));
            let file = SquareSet::file(black_box(4));
            rank | file
        })
    });
    
    group.bench_function("path_calculation", |b| {
        b.iter(|| {
            SquareSet::make_path(black_box(Square::A1), black_box(Square::H8))
        })
    });
    
    group.bench_function("iterator", |b| {
        let set = SquareSet::all();
        b.iter(|| {
            let mut count = 0;
            for _square in black_box(set) {
                count += 1;
            }
            count
        })
    });
    
    group.bench_function("occupancy", |b| {
        let mut board = Board::new();
        // Set up a typical chess position
        board[Square::A1] = Piece::R;
        board[Square::B1] = Piece::N;
        board[Square::C1] = Piece::B;
        board[Square::D1] = Piece::Q;
        board[Square::E1] = Piece::K;
        board[Square::F1] = Piece::B;
        board[Square::G1] = Piece::N;
        board[Square::H1] = Piece::R;
        for file in 0..8 {
            board[Square::make_square(file, 1)] = Piece::P;
        }
        
        board[Square::A8] = Piece::r;
        board[Square::B8] = Piece::n;
        board[Square::C8] = Piece::b;
        board[Square::D8] = Piece::q;
        board[Square::E8] = Piece::k;
        board[Square::F8] = Piece::b;
        board[Square::G8] = Piece::n;
        board[Square::H8] = Piece::r;
        for file in 0..8 {
            board[Square::make_square(file, 6)] = Piece::p;
        }
        
        b.iter(|| {
            occupancy(black_box(&board))
        })
    });
    
    group.bench_function("bitwise_operations", |b| {
        let set1 = SquareSet::rank(0) | SquareSet::rank(7);
        let set2 = SquareSet::file(0) | SquareSet::file(7);
        
        b.iter(|| {
            let union = black_box(set1) | black_box(set2);
            let intersection = black_box(set1) & black_box(set2);
            let difference = union - intersection;
            difference.len()
        })
    });
    
    group.finish();
}

criterion_group!(benches, benchmark_square_set_operations);
criterion_main!(benches);
