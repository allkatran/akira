#include "views/controller_picker_view.hpp"

#include <string>
#include <vector>

#include "views/battery_pips.hpp"
#include "input/pad_art.hpp"

#include "ui/theme.hpp"

#include <borealis/core/i18n.hpp>

using namespace brls::literals;
using akira::input::PadDescription;
using akira::input::PadPathKind;

namespace {

bool Differ(const PadDescription& a, const PadDescription& b)
{
    if (a.kind != b.kind)
        return true;
    if (a.caps.analog_triggers != b.caps.analog_triggers)
        return true;
    if (a.caps.touchpad != b.caps.touchpad)
        return true;
    return false;
}

std::vector<std::string> CapabilityLines(const PadDescription& d)
{
    std::vector<std::string> out;

    if (d.caps.analog_triggers) out.emplace_back("analog triggers");
    if (d.caps.touchpad)        out.emplace_back("touchpad");
    if (out.empty())            out.emplace_back("digital triggers");

    return out;
}

std::string SlotLine(HidNpadIdType npad)
{
    if (npad == HidNpadIdType_Handheld)
        return "Handheld";
    return "Player " + std::to_string((int)npad + 1);
}

} // namespace

bool ControllerPickerView::worthAsking(const std::vector<PadDescription>& pads)
{
    if (pads.size() < 2)
        return false;

    for (std::size_t i = 1; i < pads.size(); i++) {
        if (Differ(pads[0], pads[i]))
            return true;
    }
    return false;
}

ControllerPickerView::ControllerPickerView(std::vector<PadDescription> pads,
                                          OnChosen onChosen, Describe describe)
    : m_pads(std::move(pads))
    , m_onChosen(std::move(onChosen))
    , m_describe(std::move(describe))
{
    auto theme = brls::Application::getTheme();

    setAxis(brls::Axis::COLUMN);
    setJustifyContent(brls::JustifyContent::CENTER);
    setAlignItems(brls::AlignItems::CENTER);
    setGrow(1.0f);
    setWidthPercentage(100.0f);
    setHeightPercentage(100.0f);
    setBackgroundColor(nvgRGB(14, 18, 26));

    auto* prompt = new brls::Box();
    prompt->setAxis(brls::Axis::ROW);
    prompt->setJustifyContent(brls::JustifyContent::CENTER);
    prompt->setAlignItems(brls::AlignItems::CENTER);
    prompt->setMarginBottom(34);
    addView(prompt);

    auto* before = new brls::Label();
    before->setText("Press");
    before->setFontSize(22);
    before->setTextColor(nvgRGB(232, 238, 245));
    prompt->addView(before);

    auto* lBtn = new brls::Image();
    lBtn->setImageFromRes("img/buttons/l.png");
    lBtn->setWidth(62);
    lBtn->setHeight(46);
    lBtn->setMarginLeft(12);
    prompt->addView(lBtn);

    auto* plus = new brls::Label();
    plus->setText("+");
    plus->setFontSize(20);
    plus->setTextColor(nvgRGB(140, 155, 172));
    plus->setMarginLeft(6);
    plus->setMarginRight(6);
    prompt->addView(plus);

    auto* rBtn = new brls::Image();
    rBtn->setImageFromRes("img/buttons/r.png");
    rBtn->setWidth(62);
    rBtn->setHeight(46);
    rBtn->setMarginRight(12);
    prompt->addView(rBtn);

    auto* after = new brls::Label();
    after->setText("on the controller you'd like to use");
    after->setFontSize(22);
    after->setTextColor(nvgRGB(232, 238, 245));
    prompt->addView(after);

    m_row = new brls::Box();
    m_row->setAxis(brls::Axis::ROW);
    m_row->setJustifyContent(brls::JustifyContent::CENTER);
    m_row->setAlignItems(brls::AlignItems::CENTER);
    addView(m_row);

    buildCards();


    auto* hintBar = new brls::Box();
    hintBar->setAxis(brls::Axis::ROW);
    hintBar->setJustifyContent(brls::JustifyContent::CENTER);
    hintBar->setAlignItems(brls::AlignItems::CENTER);
    hintBar->setPaddingLeft(26);
    hintBar->setPaddingRight(26);
    hintBar->setPaddingTop(6);
    hintBar->setPaddingBottom(6);
    hintBar->setCornerRadius(12);
    hintBar->setBackgroundColor(nvgRGBA(255, 255, 255, 20));
    hintBar->setBorderColor(nvgRGBA(255, 255, 255, 38));
    hintBar->setBorderThickness(1.5f);
    hintBar->setMarginTop(34);

    auto* hints = new brls::Hints();
    hints->setHintFontSizes(34.0f, 28.0f);
    hintBar->addView(hints);
    addView(hintBar);

    setFocusable(true);

    registerAction("Continue", brls::ControllerButton::BUTTON_A,
        [this](brls::View*) {
            accept();
            return true;
        });

    setActionAvailable(brls::ControllerButton::BUTTON_A, false);

    registerAction("Back", brls::ControllerButton::BUTTON_B,
        [this](brls::View*) {
            cancel();
            return true;
        });
}

void ControllerPickerView::accept()
{
    if (m_done)
        return;

    if (m_selected >= 0 && m_selected < (int)m_pads.size()) {
        brls::Logger::info("ControllerPicker: chose {} on npad {}",
            m_pads[m_selected].label, (int)m_pads[m_selected].npad);
        choose((std::size_t)m_selected);
        return;
    }

    brls::Logger::info("ControllerPicker: continuing on the default pad");
    m_done = true;
    if (m_onChosen)
        m_onChosen(kNoChoice);
}

void ControllerPickerView::cancel()
{
    if (m_done)
        return;

    brls::Logger::info("ControllerPicker: backed out");
    m_done = true;
    if (m_onChosen)
        m_onChosen(kCancelled);
}

void ControllerPickerView::buildCards()
{
    auto theme = brls::Application::getTheme();
    (void)theme;

    m_states.resize(m_pads.size());
    m_seen_clear.assign(m_pads.size(), false);

    constexpr float kCanvas   = 1280.0f;
    constexpr float kGutter   = 28.0f;
    constexpr float kMaxCard  = 250.0f;
    constexpr float kMinCard  = 156.0f;

    const float count    = (float)(m_pads.size() > 0 ? m_pads.size() : 1);
    const float budget   = (kCanvas - 60.0f) / count - kGutter;
    const float cardW    = budget > kMaxCard ? kMaxCard : (budget < kMinCard ? kMinCard : budget);
    const float scale    = cardW / kMaxCard;

    const float artW     = 128.0f;
    const float artH     = 128.0f;

    for (std::size_t i = 0; i < m_pads.size(); i++) {
        padInitialize(&m_states[i], m_pads[i].npad);
        padUpdate(&m_states[i]);

        auto* card = new brls::Box();
        card->setAxis(brls::Axis::COLUMN);
        card->setAlignItems(brls::AlignItems::CENTER);
        card->setJustifyContent(brls::JustifyContent::CENTER);
        card->setWidth(cardW);
        card->setHeight(258);
        card->setMarginLeft(14);
        card->setMarginRight(14);
        card->setBackgroundColor(nvgRGBA(255, 255, 255, 16));
        card->setCornerRadius(14);
        card->setBorderColor(nvgRGBA(255, 255, 255, 30));
        card->setBorderThickness(1.5f);

        auto* art = new brls::Image();
        art->setImageFromRes(akira::input::PadArtPath(m_pads[i]));
        art->setScalingType(brls::ImageScalingType::FIT);
        art->setWidth(artW);
        art->setHeight(artH);
        art->setMarginBottom(14);
        card->addView(art);

        auto* name = new brls::Label();
        name->setText(m_pads[i].label);
        name->setFontSize(scale < 0.8f ? 19 : 23);
        name->setTextColor(nvgRGB(232, 238, 245));
        name->setMarginBottom(4);
        card->addView(name);

        auto* slot = new brls::Label();
        slot->setText(SlotLine(m_pads[i].npad));
        slot->setFontSize(17);
        slot->setTextColor(nvgRGB(140, 155, 172));
        slot->setMarginBottom(8);
        card->addView(slot);

        const auto capLines = CapabilityLines(m_pads[i]);
        for (std::size_t c = 0; c < capLines.size(); c++) {
            auto* caps = new brls::Label();
            caps->setText(capLines[c]);
            caps->setFontSize(scale < 0.8f ? 14 : 16);
            caps->setTextColor(nvgRGB(140, 155, 172));
            caps->setMarginBottom(c + 1 == capLines.size() ? 10 : 2);
            card->addView(caps);
        }

        if (m_pads[i].caps.battery) {
            const bool split = akira::input::PadHasTwoBatteries(m_pads[i]);
            card->addView(new BatteryPips(m_pads[i].npad, split, split ? 78 : 44, 18));
        }

        m_cards.push_back(card);
        m_row->addView(card);
    }
}

bool ControllerPickerView::SameSet(const std::vector<PadDescription>& a,
                                   const std::vector<PadDescription>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); i++) {
        if (a[i].npad != b[i].npad || a[i].kind != b[i].kind)
            return false;
    }
    return true;
}

void ControllerPickerView::rebuild(std::vector<PadDescription> fresh)
{
    bool         hadSelection = m_selected >= 0 && m_selected < (int)m_pads.size();
    PadPathKind  selKind      = hadSelection ? m_pads[m_selected].kind : PadPathKind::JoyCon;
    HidNpadIdType selNpad     = hadSelection ? m_pads[m_selected].npad : HidNpadIdType_No1;
    uint16_t     selVid       = hadSelection ? m_pads[m_selected].vendor_id  : 0;
    uint16_t     selPid       = hadSelection ? m_pads[m_selected].product_id : 0;

    m_pads = std::move(fresh);
    m_cards.clear();
    m_row->clearViews();
    buildCards();

    m_selected = carryOverSelection(hadSelection, selNpad, selKind, selVid, selPid);
    setActionAvailable(brls::ControllerButton::BUTTON_A, m_selected >= 0);
}

int ControllerPickerView::carryOverSelection(bool hadSelection, HidNpadIdType selNpad,
                                             PadPathKind selKind, uint16_t selVid,
                                             uint16_t selPid) const
{
    if (!hadSelection)
        return -1;

    for (std::size_t i = 0; i < m_pads.size(); i++) {
        if (m_pads[i].npad == selNpad && m_pads[i].kind == selKind)
            return (int)i;
    }

    if (selVid != 0) {
        for (std::size_t i = 0; i < m_pads.size(); i++) {
            if (m_pads[i].vendor_id == selVid && m_pads[i].product_id == selPid)
                return (int)i;
        }
    }

    if (selKind == PadPathKind::JoyCon) {
        int count = 0, only = -1;
        for (std::size_t i = 0; i < m_pads.size(); i++) {
            if (m_pads[i].kind == PadPathKind::JoyCon) { count++; only = (int)i; }
        }
        if (count == 1)
            return only;
    }

    return -1;
}

void ControllerPickerView::choose(std::size_t index)
{
    if (m_done || index >= m_pads.size())
        return;

    m_done = true;
    if (m_onChosen)
        m_onChosen(m_pads[index].npad);
}

void ControllerPickerView::draw(NVGcontext* vg, float x, float y, float width, float height,
                                brls::Style style, brls::FrameContext* ctx)
{
    Box::draw(vg, x, y, width, height, style, ctx);

    if (m_selected >= 0 && m_selected < (int)m_cards.size()) {
        brls::Box* card = m_cards[m_selected];
        const float cx = card->getX(), cy = card->getY();
        const float cw = card->getWidth(), ch = card->getHeight();

        for (int pass = 3; pass >= 0; pass--) {
            const float grow  = 3.0f + pass * 4.0f;
            const int   alpha = pass == 0 ? 255 : 46 - pass * 10;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx - grow, cy - grow, cw + grow * 2, ch + grow * 2, 14.0f + grow);
            nvgStrokeColor(vg, nvgRGBA(64, 208, 122, (unsigned char)alpha));
            nvgStrokeWidth(vg, pass == 0 ? 3.0f : 6.0f);
            nvgStroke(vg);
        }
    }

    if (m_done)
        return;

    m_frames++;

    if (m_describe && (m_frames % 30) == 0) {
        auto fresh = m_describe();
        if (!SameSet(fresh, m_pads)) {
            brls::Logger::info("ControllerPicker: pads changed, {} -> {}",
                               m_pads.size(), fresh.size());
            rebuild(std::move(fresh));
            return;
        }
    }

    if (m_frames > 1800) {
        brls::Logger::warning("ControllerPicker: no input in ~30s, continuing");
        accept();
        return;
    }

    for (std::size_t i = 0; i < m_states.size(); i++) {
        padUpdate(&m_states[i]);

        const u64 held = padGetButtons(&m_states[i]);

        if ((m_frames % 120) == 1) {
            brls::Logger::info("ControllerPicker: npad {} connected={} held=0x{:x}",
                               (int)m_pads[i].npad, padIsConnected(&m_states[i]) ? 1 : 0,
                               (unsigned long long)held);
        }

        constexpr u64 kShoulder = HidNpadButton_L  | HidNpadButton_R;
        constexpr u64 kTrigger   = HidNpadButton_ZL | HidNpadButton_ZR;

        const bool picking = ((held & kShoulder) == kShoulder) ||
                             ((held & kTrigger)  == kTrigger);

        if (!picking) {
            m_seen_clear[i] = true;
            continue;
        }

        if (!m_seen_clear[i])
            continue;

        if (m_selected != (int)i) {
            m_selected = (int)i;
            setActionAvailable(brls::ControllerButton::BUTTON_A, true);
            brls::Logger::info("ControllerPicker: selected {} on npad {}",
                m_pads[i].label, (int)m_pads[i].npad);
        }
        return;
    }
}
