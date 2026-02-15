use std::time::Instant;

#[tokio::main]
async fn main() {
    let start = Instant::now();
    
    let mut handles = Vec::new();
    for i in 0..10000 {
        handles.push(tokio::spawn(async move {
            i * i
        }));
    }
    
    let mut errors = 0;
    for (i, handle) in handles.into_iter().enumerate() {
        let r = handle.await.unwrap();
        if r != i * i {
            errors += 1;
        }
    }
    
    let elapsed = start.elapsed();
    
    println!("Completed: {} errors", errors);
    println!("Time: {:.2} ms", elapsed.as_secs_f64() * 1000.0);
    println!("Per operation: {:.2} μs", elapsed.as_secs_f64() * 1000000.0 / 10000.0);
    
    if errors == 0 {
        println!("✅ PASS");
    } else {
        println!("❌ FAIL");
    }
}
