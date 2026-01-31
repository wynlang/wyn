use std::time::Instant;
use tokio::task;

fn compute(n: usize) -> usize {
    n * n
}

async fn benchmark(count: usize) {
    let start = Instant::now();
    
    let mut handles = Vec::with_capacity(count);
    let mut results = vec![0; count];
    
    for i in 0..count {
        let handle = task::spawn(async move {
            compute(i)
        });
        handles.push(handle);
    }
    
    for (i, handle) in handles.into_iter().enumerate() {
        results[i] = handle.await.unwrap();
    }
    
    let elapsed = start.elapsed();
    println!("Rust {}: {} ms", count, elapsed.as_millis());
}

#[tokio::main]
async fn main() {
    benchmark(10000).await;
    benchmark(100000).await;
    benchmark(1000000).await;
    benchmark(10000000).await;
}
