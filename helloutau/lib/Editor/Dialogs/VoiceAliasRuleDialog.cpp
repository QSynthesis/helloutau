#include "VoiceAliasRuleDialog.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    VoiceAliasRuleDialog::VoiceAliasRuleDialog(Mode mode, const QList<Group> &groups,
                                               QWidget *parent)
        : QDialog(parent), m_mode(mode), m_groups(groups) {
        setWindowTitle(mode == Rename ? tr("Rename Aliases") : tr("Duplicate Entries with Rule"));

        using Rule = kit::VoiceAliasRule;
        m_kind = new QComboBox();
        m_kind->addItem(tr("Add prefix"), Rule::AddPrefix);
        m_kind->addItem(tr("Add suffix"), Rule::AddSuffix);
        m_kind->addItem(tr("Replace text"), Rule::Replace);
        m_kind->addItem(tr("Remove prefix"), Rule::RemovePrefix);
        m_kind->addItem(tr("Remove suffix"), Rule::RemoveSuffix);
        m_kind->setCurrentIndex(m_kind->findData(Rule::AddSuffix));
        m_text = new QLineEdit();
        m_replacement = new QLineEdit();
        m_replacementLabel = new QLabel(tr("Re&place with:"));
        m_replacementLabel->setBuddy(m_replacement);

        auto form = new QFormLayout();
        form->addRow(tr("&Rule:"), m_kind);
        form->addRow(tr("&Text:"), m_text);
        form->addRow(m_replacementLabel, m_replacement);

        m_preview = new QTreeWidget();
        m_preview->setRootIsDecorated(false);
        m_preview->setUniformRowHeights(true);
        m_preview->setHeaderLabels(
            {tr("Current"), mode == Rename ? tr("New alias") : tr("Copy alias"), tr("Problem")});
        m_preview->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        m_ok = buttons->button(QDialogButtonBox::Ok);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(m_preview, 1);
        layout->addWidget(buttons);

        connect(m_kind, &QComboBox::currentIndexChanged, this,
                &VoiceAliasRuleDialog::updatePreview);
        connect(m_text, &QLineEdit::textChanged, this, &VoiceAliasRuleDialog::updatePreview);
        connect(m_replacement, &QLineEdit::textChanged, this, &VoiceAliasRuleDialog::updatePreview);
        updatePreview();
        resize(560, 420);
    }

    VoiceAliasRuleDialog::~VoiceAliasRuleDialog() = default;

    kit::VoiceAliasRule VoiceAliasRuleDialog::rule() const {
        kit::VoiceAliasRule rule;
        rule.kind = kit::VoiceAliasRule::Kind(m_kind->currentData().toInt());
        rule.text = m_text->text();
        rule.replacement = m_replacement->text();
        return rule;
    }

    QList<QList<kit::VoiceAliasRule::Change>> VoiceAliasRuleDialog::changes() const {
        const auto current = rule();
        QList<QList<kit::VoiceAliasRule::Change>> result;
        for (const auto &group : std::as_const(m_groups)) {
            result.push_back(current.plan(group.entries, group.selected, m_mode == Duplicate));
        }
        return result;
    }

    QComboBox *VoiceAliasRuleDialog::kindBox() const {
        return m_kind;
    }

    QLineEdit *VoiceAliasRuleDialog::textEdit() const {
        return m_text;
    }

    QLineEdit *VoiceAliasRuleDialog::replacementEdit() const {
        return m_replacement;
    }

    QTreeWidget *VoiceAliasRuleDialog::preview() const {
        return m_preview;
    }

    QPushButton *VoiceAliasRuleDialog::okButton() const {
        return m_ok;
    }

    // An unchanged renaming is listed without a problem and is not counted as a change.
    void VoiceAliasRuleDialog::updatePreview() {
        const bool replace = m_kind->currentData().toInt() == kit::VoiceAliasRule::Replace;
        m_replacementLabel->setVisible(replace);
        m_replacement->setVisible(replace);

        m_preview->clear();
        const auto warning = style()->standardIcon(QStyle::SP_MessageBoxWarning);
        int changed = 0;
        bool problems = false;
        for (const auto &group : changes()) {
            for (const auto &change : group) {
                auto item =
                    new QTreeWidgetItem(m_preview, {change.from, change.to, change.problem});
                if (!change.problem.isEmpty()) {
                    item->setIcon(2, warning);
                    problems = true;
                } else if (change.to != change.from) {
                    ++changed;
                }
            }
        }
        m_ok->setEnabled(!m_text->text().isEmpty() && changed > 0 && !problems);
    }

}
