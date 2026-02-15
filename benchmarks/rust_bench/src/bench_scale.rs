use std::time::Instant;

#[tokio::main]
async fn main() {
    benchmark(10000).await;
    benchmark(100000).await;
    benchmark(1000000).await;
    benchmark(10000000).await;
}

async fn benchmark(count: usize) {
    let start = Instant::now();
    
    let mut handles = Vec::new();
    for i in 0..count {
        handles.push(tokio::spawn(async move {
            i * i
        }));
    }
    
    for handle in handles {
        handle.await.unwrap();
    }
    
    let elapsed = start.elapsed();
    println!("{},{}", count, elapsed.as_millis());
}
