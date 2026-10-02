use perastage_mcp::{PerastageMcp, ProcessCliBackend};
use rmcp::{ServiceExt, transport::stdio};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let server = PerastageMcp::new(ProcessCliBackend::from_environment());
    server.serve(stdio()).await?.waiting().await?;
    Ok(())
}
