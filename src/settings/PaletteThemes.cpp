#include "PaletteThemes.h"
#include "Settings.h"

namespace {

QVariantMap theme(double brightnessBias, std::initializer_list<uint32_t> colors, bool disabledLCDColor,
                  double hueBias, double hueBiasStrength)
{
    QVariantList colorList;
    for (uint32_t color : colors) {
        colorList << QVariant::fromValue<quint32>(color);
    }
    return {
        {QStringLiteral("BrightnessBias"), brightnessBias},
        {QStringLiteral("Colors"), colorList},
        {QStringLiteral("DisabledLCDColor"), disabledLCDColor},
        {QStringLiteral("HueBias"), hueBias},
        {QStringLiteral("HueBiasStrength"), hueBiasStrength},
        {QStringLiteral("Manual"), false},
    };
}

} // namespace

QVariantMap defaultPaletteThemes()
{
    // Keep in sync with the GBThemes dictionary in Cocoa/GBApp.m.
    return {
        {QStringLiteral("Canyon"), theme(0.1227009965823247, {0xff0c1e20, 0xff122b91, 0xff466aa2, 0xfff1efae, 0xfff1efae}, false, 0.01782661816105247, 1)},
        {QStringLiteral("Desert"), theme(0.0, {0xff302f3e, 0xff576674, 0xff839ba4, 0xffb1d0d2, 0xffb7d7d8}, true, 0.10087773904382469, 0.062142056772908363)},
        {QStringLiteral("Evening"), theme(-0.10168700106441975, {0xff362601, 0xff695518, 0xff899853, 0xffa6e4ae, 0xffa9eebb}, true, 0.60027079191058874, 0.33816297305747867)},
        {QStringLiteral("Fog"), theme(0.0, {0xff373c34, 0xff737256, 0xff9da386, 0xffc3d2bf, 0xffc7d8c6}, true, 0.55750435756972117, 0.18424738545816732)},
        {QStringLiteral("Green Slate"), theme(0.2210012227296829, {0xff343117, 0xff6a876f, 0xff98b4a1, 0xffc3daca, 0xffc8decf}, true, 0.1887667975388467, 0.1272283345460892)},
        {QStringLiteral("Green Tea"), theme(-0.4946326622596153, {0xff1a1d08, 0xff1d5231, 0xff3b9774, 0xff97e4c6, 0xffa9eed1}, true, 0.1912955007245464, 0.3621708039314516)},
        {QStringLiteral("Lavender"), theme(0.10072476038566, {0xff2b2a3a, 0xff8c507c, 0xffbf82a8, 0xffe9bcce, 0xffeec3d3}, true, 0.7914529587142169, 0.2498168498277664)},
        {QStringLiteral("Magic Eggplant"), theme(0.0, {0xff3c2136, 0xff942e84, 0xffc7699d, 0xfff1e4b0, 0xfff6f9b2}, true, 0.87717878486055778, 0.65018052788844627)},
        {QStringLiteral("Mystic Blue"), theme(-0.3291049897670746, {0xff3b2306, 0xffa27807, 0xffd1b523, 0xfff6ebbe, 0xfffaf1e4}, true, 0.5282051088288426, 0.7699633836746216)},
        {QStringLiteral("Pink Pop"), theme(0.624908447265625, {0xff28140a, 0xff7c42cb, 0xffaa83de, 0xffd1ceeb, 0xffd5d8ec}, true, 0.9477411056868732, 0.80024421215057373)},
        {QStringLiteral("Radioactive Pea"), theme(-0.48079556772908372, {0xff215200, 0xff1f7306, 0xff169e34, 0xff03ceb8, 0xff00d4d1}, true, 0.3795131972111554, 0.34337649402390436)},
        {QStringLiteral("Rose"), theme(0.2727272808551788, {0xff001500, 0xff4e1fae, 0xff865ac4, 0xffb7e6d3, 0xffbdffd4}, true, 0.9238900924101472, 0.9957716464996338)},
        {QStringLiteral("Seaweed"), theme(-0.28532744023904377, {0xff3f0015, 0xff426532, 0xff58a778, 0xff95e0df, 0xffa0e7ee}, true, 0.2694067480079681, 0.51565612549800799)},
        {QStringLiteral("Twilight"), theme(-0.091789093625498031, {0xff3f0015, 0xff461286, 0xff6254bd, 0xff97d3e9, 0xffa0e7ee}, true, 0.0, 0.49710532868525897)},
    };
}

QColor themeColorFromInt(uint32_t c)
{
    return QColor(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF);
}

uint32_t themeColorToInt(const QColor &color)
{
    return uint32_t(color.red()) | (uint32_t(color.green()) << 8) | (uint32_t(color.blue()) << 16) | 0xFF000000u;
}

const GB_palette_t *currentUserPalette()
{
    Settings &settings = Settings::instance();
    switch (settings.intValue(QStringLiteral("GBColorPalette"))) {
        case 1: return &GB_PALETTE_DMG;
        case 2: return &GB_PALETTE_MGB;
        case 3: return &GB_PALETTE_GBL;
        default: return &GB_PALETTE_GREY;
        case -1: {
            static GB_palette_t customPalette;
            const QVariantList colors = settings.mapValue(QStringLiteral("GBThemes"))
                                            .value(settings.stringValue(QStringLiteral("GBCurrentTheme")))
                                            .toMap()
                                            .value(QStringLiteral("Colors"))
                                            .toList();
            if (colors.size() == 5) {
                for (int i = 0; i < 5; i++) {
                    uint32_t c = colors[i].toUInt();
                    customPalette.colors[i] = {uint8_t(c), uint8_t(c >> 8), uint8_t(c >> 16)};
                }
            }
            return &customPalette;
        }
    }
}
