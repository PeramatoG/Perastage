/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include "mvr_import_resource_resolver.h"
#include "mvr_read_service.h"

namespace mvr {

// Adapts application resource and model services to the shared parser.
MvrReadEnvironment
MakeApplicationReadEnvironment(MvrImportResourceResolver &resolver);

} // namespace mvr
