/*
 * File: pdfparser.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of PDF text extraction and parsing utilities for fixtures and trusses.
 */

#include "pdfparser.h"
#include "fixturemanager.h"

#include <QProcess>
#include <QTextStream>
#include <QRegularExpression>

// Extract plain text from a PDF using pdftotext
bool PDFParser::extractTextFromPDF(const QString& pdfPath, QString& textOutput)
{
    QString pdftotextPath = "pdftotext"; // Assume it is in PATH

    QProcess process;
    QStringList arguments;
    arguments << "-layout" << pdfPath << "-"; // Output to stdout

    process.start(pdftotextPath, arguments);
    if (!process.waitForStarted() || !process.waitForFinished()) {
        qWarning() << "Failed to start or finish pdftotext process.";
        return false;
    }

    textOutput = QString::fromUtf8(process.readAllStandardOutput());
    return !textOutput.isEmpty();
}

void PDFParser::parseFixtureAndTrussList(const QString& textContent, QList<FixtureItem>& fixtures, QList<TrussItem>& trusses)
{
    fixtures.clear();
    trusses.clear();

    static const QRegularExpression fixtureRegex(R"((\d+)\s+(.+))");
    static const QRegularExpression lineRegex(R"((\d+)\s+(.+))");
    static const QRegularExpression metersRegex(R"((\d+(\.\d+)?)\s*m)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression positionHeaderRegex(R"((LX\d+))", QRegularExpression::CaseInsensitiveOption);

    QTextStream stream(const_cast<QString*>(&textContent), QIODevice::ReadOnly);
    QString currentSection;
    QString currentPosition;

    QMap<QString, QString> positionKeywords = {
        {"lx", "LX"},
        {"pantalla", "SCREEN"},
        {"ledscreen", "SCREEN"},
        {"pantallas laterales", "SCREEN"},
        {"frontal", "FRONTAL"},
        {"medio", "MEDIO"},
        {"trasero", "TRASERO"},
        {"telon", "TELON"},
        {"backdrop", "TELON"},
        {"laterales", "LATERALES"},
        {"suelo", "SUELO"},
        {"luces", "LUZ"},
        {"iluminacion", "LUZ"}
    };


    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QString normalizedLine = removeAccents(line).toLower();

        if (normalizedLine.contains("iluminacion") || normalizedLine.contains("robotica") || normalizedLine.contains("convencional")) {
            currentSection = "Fixtures";
            currentPosition.clear();
            continue;
        }
        if (normalizedLine.contains("rigging")) {
            currentSection = "Rigging";
            currentPosition.clear();
            continue;
        }
        // New: detect when leaving Fixtures/Rigging
        if (normalizedLine.contains("sonido") || normalizedLine.contains("audio") || normalizedLine.contains("control de p.a.") ||
            normalizedLine.contains("monitores") || normalizedLine.contains("microfonia") || normalizedLine.contains("video") ||
            normalizedLine.contains("pantalla") || normalizedLine.contains("realizacion")) {
            currentSection.clear();
            currentPosition.clear();
            continue;
        }

        // Detect position headers like LX1, LX2, etc.
        QRegularExpressionMatch positionMatch = positionHeaderRegex.match(line);
        if (positionMatch.hasMatch()) {
            currentPosition = positionMatch.captured(1).toUpper();
            // Some documents start listing fixtures directly under position headers
            // without a preceding section like "Iluminacion". In that case the
            // parser never enters the Fixtures section and those items are ignored.
            // Assume that any detected position header implies fixture listing when
            // no section has been set yet.
            if (currentSection.isEmpty())
                currentSection = "Fixtures";
            continue;
        }

        if ((currentSection == "Fixtures" || currentSection == "Rigging") &&
            (line.startsWith("-") || (!line.isEmpty() && line[0].isDigit()))) {

            QRegularExpressionMatch match = lineRegex.match(line);
            if (!match.hasMatch())
                continue;

            int quantity = match.captured(1).toInt();
            QString description = match.captured(2).trimmed();

            if (currentSection == "Fixtures") {
                FixtureItem item;
                item.quantity = quantity;
                item.model = description;

                // Get fixtures atributes from dictionary
                FixtureManager::resolveFixtureAttributes(item);

                item.positionName = currentPosition;
                fixtures.append(item);
            }
            else if (currentSection == "Rigging") {
                if (normalizedLine.contains("motor")) {
                    QString motorType = "Motor";
                    if (normalizedLine.contains("1to") || normalizedLine.contains("1t") || normalizedLine.contains("1000kg")) {
                        motorType += " 1T";
                    } else if (normalizedLine.contains("2t") || normalizedLine.contains("2000kg")) {
                        motorType += " 2T";
                    } else if (normalizedLine.contains("500kg")) {
                        motorType += " 0.5T";
                    } else {
                        motorType += " Unknown";
                    }

                    bool found = false;
                    for (FixtureItem& existing : fixtures) {
                        if (existing.model.compare(motorType, Qt::CaseInsensitive) == 0) {
                            existing.quantity += quantity;
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        FixtureItem motorItem;
                        motorItem.quantity = quantity;
                        motorItem.model = motorType;
                        motorItem.gdtfSpec = "";
                        motorItem.type = "Motor";
                        fixtures.append(motorItem);
                    }
                }
                else if (normalizedLine.contains("truss")) {
                    QString basePosition = "EXTRA";

                    for (auto it = positionKeywords.constBegin(); it != positionKeywords.constEnd(); ++it) {
                        if (normalizedLine.contains(it.key())) {
                            basePosition = it.value();
                            break;
                        }
                    }

                    double lengthMeters = 0.0;
                    QRegularExpressionMatch metersMatch = metersRegex.match(description);
                    if (metersMatch.hasMatch()) {
                        lengthMeters = metersMatch.captured(1).toDouble();
                    }

                    int startIndex = 1;
                    for (int i = 0; i < quantity; ++i) {
                        TrussItem trussItem;
                        trussItem.name = description;
                        trussItem.lengthMeters = lengthMeters;
                        trussItem.model = ""; // Currently no dictionary for trusses

                        if (basePosition == "LX") {
                            trussItem.position = QString("LX%1").arg(startIndex++);
                        } else {
                            trussItem.position = basePosition;
                        }

                        trusses.append(trussItem);
                    }
                }
            }
        }
    }
}

// Helper function to remove accents
QString PDFParser::removeAccents(const QString& input)
{
    QString normalized = input.normalized(QString::NormalizationForm_D);
    QString output;
    for (QChar c : normalized) {
        if (c.category() != QChar::Mark_NonSpacing)
            output.append(c);
    }
    return output;
}




