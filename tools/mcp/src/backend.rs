use serde::Serialize;
use serde_json::{Value, json};
use std::ffi::{OsStr, OsString};
use std::process::{Command, Output};

#[derive(Clone, Debug, Serialize)]
pub struct CliResponse {
    pub success: bool,
    pub exit_code: Option<i32>,
    pub result: Value,
    pub stderr: String,
}

pub trait CliBackend: Send + Sync + 'static {
    fn invoke(&self, arguments: &[OsString]) -> CliResponse;
}

#[derive(Clone, Debug)]
pub struct ProcessCliBackend {
    executable: OsString,
}

impl ProcessCliBackend {
    pub fn new(executable: impl Into<OsString>) -> Self {
        Self {
            executable: executable.into(),
        }
    }

    pub fn from_environment() -> Self {
        Self::new(
            std::env::var_os("PERASTAGE_CLI").unwrap_or_else(|| OsString::from("perastage-cli")),
        )
    }

    fn response(output: Output) -> CliResponse {
        let stdout = String::from_utf8_lossy(&output.stdout).into_owned();
        let stderr = String::from_utf8_lossy(&output.stderr)
            .trim_end()
            .to_owned();
        let result = serde_json::from_str(stdout.trim()).unwrap_or_else(|_| {
            json!({
                "raw_stdout": stdout,
                "adapter_diagnostic": "Perastage CLI did not return structured JSON."
            })
        });
        CliResponse {
            success: output.status.success(),
            exit_code: output.status.code(),
            result,
            stderr,
        }
    }
}

impl CliBackend for ProcessCliBackend {
    fn invoke(&self, arguments: &[OsString]) -> CliResponse {
        match Command::new(&self.executable).args(arguments).output() {
            Ok(output) => Self::response(output),
            Err(error) => CliResponse {
                success: false,
                exit_code: None,
                result: json!({
                    "adapter_diagnostic": "Perastage CLI is unavailable.",
                    "kind": error.kind().to_string()
                }),
                stderr: error.to_string(),
            },
        }
    }
}

pub fn args(values: &[&str]) -> Vec<OsString> {
    values.iter().map(OsStr::new).map(OsString::from).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unavailable_cli_returns_a_structured_adapter_diagnostic() {
        let backend = ProcessCliBackend::new("definitely-not-a-perastage-executable");
        let response = backend.invoke(&args(&["capabilities", "--json"]));
        assert!(!response.success);
        assert_eq!(response.exit_code, None);
        assert_eq!(
            response.result["adapter_diagnostic"],
            "Perastage CLI is unavailable."
        );
        assert!(!response.stderr.is_empty());
    }
}
