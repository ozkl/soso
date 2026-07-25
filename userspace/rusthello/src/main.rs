use std::collections::{BTreeMap, HashMap};
use std::fs::File;
use std::io::Read;

fn test_vec() {
    let mut numbers = vec![5, 1, 4, 2, 3];
    numbers.push(6);
    numbers.sort();

    assert_eq!(numbers, [1, 2, 3, 4, 5, 6]);
    assert_eq!(numbers.iter().sum::<i32>(), 21);

    println!("Vec: {numbers:?}");
}

fn test_hash_map() {
    let mut languages = HashMap::new();
    languages.insert("Rust", 2015);
    languages.insert("C", 1972);
    languages.insert("C++", 1985);

    assert_eq!(languages.get("Rust"), Some(&2015));
    assert!(languages.contains_key("C"));
    assert_eq!(languages.len(), 3);

    let mut names: Vec<_> = languages.keys().copied().collect();
    names.sort();
    println!("HashMap keys: {names:?}");
}

fn test_btree_map() {
    let scores = BTreeMap::from([
        ("alice", 30),
        ("bob", 20),
        ("charlie", 10),
    ]);

    let entries: Vec<_> = scores
        .iter()
        .map(|(name, score)| format!("{name}={score}"))
        .collect();

    assert_eq!(entries, ["alice=30", "bob=20", "charlie=10"]);
    println!("BTreeMap: {}", entries.join(", "));
}

fn test_string() {
    let words = ["Hello", "from", "Rust", "std"];
    let message = words.join(" ");

    assert_eq!(message, "Hello from Rust std");
    assert_eq!(message.split_whitespace().count(), 4);

    println!("String: {message}");
}

fn test_file() -> bool {
    let mut file = match File::open("test.txt") {
        Ok(file) => file,
        Err(error) => {
            eprintln!(
                "Could not open test.txt: {error} (error kind: {:?})",
                error.kind()
            );
            return false;
        }
    };

    let mut contents = String::new();
    if let Err(error) = file.read_to_string(&mut contents) {
        eprintln!(
            "Could not read test.txt: {error} (error kind: {:?})",
            error.kind()
        );
        return false;
    }

    println!("test.txt contents:");
    print!("{contents}");
    if !contents.ends_with('\n') {
        println!();
    }

    true
}

fn main() {
    println!("Rust standard library smoke test");

    test_vec();
    test_hash_map();
    test_btree_map();
    test_string();

    if !test_file() {
        std::process::exit(1);
    }

    println!("All Rust std tests passed");
}
