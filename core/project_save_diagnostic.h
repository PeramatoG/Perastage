/* This file is part of Perastage. */
#pragma once
#include <string>

// GUI-independent failure categories; source evidence stays in technical diagnostics.
enum class ProjectSaveFailureKind {
  None, Serialization, MvrPreservation, Resource, ArchiveIo, Unexpected
};
struct ProjectSaveFailure {
  ProjectSaveFailureKind kind = ProjectSaveFailureKind::None;
  std::string stage;
  std::string detail;
  std::string entry;
};
