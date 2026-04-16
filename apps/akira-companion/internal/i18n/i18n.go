package i18n

import (
	"embed"
	"fmt"

	"github.com/BurntSushi/toml"
	"github.com/nicksnyder/go-i18n/v2/i18n"
	"golang.org/x/text/language"
)

//go:embed locales/*.toml
var localeFS embed.FS

var localizer *i18n.Localizer

func Init(lang string) {
	bundle := i18n.NewBundle(language.English)
	bundle.RegisterUnmarshalFunc("toml", toml.Unmarshal)

	bundle.LoadMessageFileFS(localeFS, "locales/en.toml")

	if lang != "en" {
		filename := fmt.Sprintf("locales/%s.toml", lang)
		bundle.LoadMessageFileFS(localeFS, filename)
	}

	localizer = i18n.NewLocalizer(bundle, lang)
}

func T(id string) string {
	msg, err := localizer.Localize(&i18n.LocalizeConfig{
		MessageID: id,
	})
	if err != nil {
		return id
	}
	return msg
}

func Tf(id string, data map[string]interface{}) string {
	msg, err := localizer.Localize(&i18n.LocalizeConfig{
		MessageID:    id,
		TemplateData: data,
	})
	if err != nil {
		return id
	}
	return msg
}
