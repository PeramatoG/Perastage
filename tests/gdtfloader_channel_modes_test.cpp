/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "gdtfloader.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {
std::string MakeGdtf()
{
    wxFileName tempName(wxFileName::CreateTempFileName("gdtf_modes_"));
    const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
    wxRemoveFile(tempName.GetFullPath());

    wxFFileOutputStream fileOut(outPath);
    assert(fileOut.IsOk());
    wxZipOutputStream zipOut(fileOut);

    zipOut.PutNextEntry("description.xml");
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GDTF DataVersion=\"1.2\">"
        "<FixtureType Name=\"ModeSummaryFixture\">"
        "<DMXModes>"
        "<DMXMode Name=\"Standard\">"
        "<DMXChannels>"
        "<DMXChannel Offset=\"1\">"
        "<LogicalChannel Attribute=\"Gobo2\">"
        "<ChannelFunction Name=\"Gobo2Index\" Attribute=\"Gobo2\" DMXFrom=\"0/1\">"
        "<ChannelSet Name=\"Open\" DMXFrom=\"0/1\"/>"
        "<ChannelSet Name=\"Gobo 1 Index\" DMXFrom=\"16/1\"/>"
        "</ChannelFunction>"
        "<ChannelFunction Name=\"Gobo2Rotate\" Attribute=\"Gobo2\" DMXFrom=\"128/1\">"
        "<ChannelSet Name=\"Rotate Left Fast\" DMXFrom=\"128/1\"/>"
        "<ChannelSet Name=\"Stop\" DMXFrom=\"192/1\"/>"
        "<ChannelSet Name=\"Rotate Right Fast\" DMXFrom=\"255/1\"/>"
        "</ChannelFunction>"
        "</LogicalChannel>"
        "</DMXChannel>"
        "<DMXChannel Offset=\"2\">"
        "<LogicalChannel Attribute=\"Gobo2PosRotate\">"
        "<ChannelFunction Name=\"Gobo2PosRotate\" Attribute=\"Gobo2PosRotate\" "
        "ModeMaster=\"Gobo2Rotate\" ModeFrom=\"128/1\" ModeTo=\"255/1\"/>"
        "</LogicalChannel>"
        "</DMXChannel>"
        "</DMXChannels>"
        "</DMXMode>"
        "</DMXModes>"
        "</FixtureType>"
        "</GDTF>";
    zipOut.Write(xml.data(), xml.size());
    zipOut.Close();

    return outPath;
}
}

int main()
{
    const std::string gdtfPath = MakeGdtf();
    const std::vector<GdtfChannelInfo> channels =
        GetGdtfModeChannels(gdtfPath, "Standard");
    assert(channels.size() == 2);
    assert(channels[0].channel == 1);
    assert(channels[1].channel == 2);

    const std::string& goboSummary = channels[0].function;
    assert(goboSummary.find("Gobo2") != std::string::npos);
    assert(goboSummary.find("sets: Open@0") != std::string::npos);
    assert(goboSummary.find("Rotate Left Fast@128") != std::string::npos);

    const std::string& dedicatedSummary = channels[1].function;
    assert(dedicatedSummary.find("Gobo2PosRotate") != std::string::npos);
    assert(dedicatedSummary.find("mode master: Gobo2 128-255") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove(gdtfPath, ec);
    return 0;
}
