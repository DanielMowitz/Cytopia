#include "uiManager.hxx"

#include "resourcesManager.hxx"
#include "engine.hxx"
#include "map.hxx"

#include "basics/mapEdit.hxx"
#include "basics/settings.hxx"
#include "basics/log.hxx"

#include "../ThirdParty/json.hxx"

using json = nlohmann::json;

void UIManager::init()
{
  json uiLayout;

  std::ifstream i(Settings::instance().settings.uiLayoutJSONFile);
  if (i.fail())
  {
    LOG(LOG_ERROR) << "File " << Settings::instance().settings.uiLayoutJSONFile
                   << " does not exist! Cannot load settings from INI File!";
    // Application should quit here, without textureData we can't continue
    return;
  }

  // check if json file can be parsed
  uiLayout = json::parse(i, nullptr, false);
  if (uiLayout.is_discarded())
  {
    LOG(LOG_ERROR) << "Error parsing JSON File " << Settings::instance().settings.uiLayoutJSONFile;
  }

  for (const auto &it : uiLayout.items())
  {
    std::string groupID;
    groupID = it.key();

    bool visible = false;

    for (size_t id = 0; id < uiLayout[it.key()].size(); id++)
    {

      if (!uiLayout[it.key()][id]["groupVisibility"].is_null())
      {
        visible = uiLayout[it.key()][id]["groupVisibility"].get<bool>();
      }

      if (!uiLayout[it.key()][id]["Type"].is_null())
      {
        bool toggleButton = uiLayout[it.key()][id].value("ToggleButton", false);
        bool drawFrame = uiLayout[it.key()][id].value("DrawFrame", false);
        std::string uiElementID = uiLayout[it.key()][id].value("ID", "");
        std::string actionID = uiLayout[it.key()][id].value("Action", "");
        std::string parentOf = uiLayout[it.key()][id].value("ParentOfGroup", "");
        std::string tooltipText = uiLayout[it.key()][id].value("TooltipText", "");
        std::string text = uiLayout[it.key()][id].value("Text", "");
        std::string textureID = uiLayout[it.key()][id].value("SpriteID", "");
        std::string type = uiLayout[it.key()][id].value("Type", "");

        SDL_Rect elementRect = {0, 0, 0, 0};
        elementRect.x = uiLayout[it.key()][id].value("Position_x", 0);
        elementRect.y = uiLayout[it.key()][id].value("Position_y", 0);
        elementRect.w = uiLayout[it.key()][id].value("Width", 0);
        elementRect.h = uiLayout[it.key()][id].value("Height", 0);

        std::unique_ptr<UiElement> uiElement;

        // Create the ui elements
        if (type.empty())
        {
          LOG(LOG_ERROR) << "An element without a type can not be created, check your UiLayout JSON File "
                         << Settings::instance().settings.uiLayoutJSONFile;
          continue;
        }
        else if (type == "ImageButton")
        {
          uiElement = std::make_unique<Button>(elementRect);
          uiElement->setTextureID(textureID);
        }
        else if (type == "TextButton")
        {
          uiElement = std::make_unique<Button>(elementRect);
          uiElement->setText(text);
        }
        else if (type == "Text")
        {
          uiElement = std::make_unique<Text>(elementRect);
          uiElement->setText(text);
        }
        else if (type == "Frame")
        {
          uiElement = std::make_unique<Frame>(elementRect);
        }
        else if (type == "Checkbox")
        {
          uiElement = std::make_unique<Checkbox>(elementRect);
        }
        else if (type == "ComboBox")
        {
          uiElement = std::make_unique<ComboBox>(elementRect);
          uiElement->setText(text);
        }

        uiElement->setVisibility(visible);
        uiElement->setTooltipText(tooltipText);
        uiElement->setActionID(actionID);
        uiElement->setParentID(parentOf);
        uiElement->setGroupID(groupID);
        uiElement->setToggleButton(toggleButton);
        uiElement->setUIElementID(uiElementID);
        uiElement->drawImageButtonFrame(drawFrame);

        if (!parentOf.empty())
        {
          uiElement->registerCallbackFunction(Signal::slot(this, &UIManager::toggleGroupVisibility));
        }
        if (actionID == "RaiseTerrain")
        {
          uiElement->registerCallbackFunction([]() {
            terrainEditMode == TerrainEdit::RAISE ? terrainEditMode = TerrainEdit::NONE : terrainEditMode = TerrainEdit::RAISE;
            terrainEditMode == TerrainEdit::NONE ? highlightSelection = false : highlightSelection = true;
          });
        }
        else if (actionID == "LowerTerrain")
        {
          uiElement->registerCallbackFunction([]() {
            terrainEditMode == TerrainEdit::LOWER ? terrainEditMode = TerrainEdit::NONE : terrainEditMode = TerrainEdit::LOWER;
            terrainEditMode == TerrainEdit::NONE ? highlightSelection = false : highlightSelection = true;
          });
        }
        else if (actionID == "QuitGame")
        {
          uiElement->registerCallbackFunction(Signal::slot(Engine::instance(), &Engine::quitGame));
        }
        else if (actionID == "Demolish")
        {
          uiElement->registerCallbackFunction([]() {
            demolishMode = !demolishMode;
            demolishMode ? highlightSelection = true : highlightSelection = false;
          });
        }
        else if (actionID == "ChangeTileType")
        {
          std::string type = uiLayout[it.key()][id].value("TileType", "");
          uiElement->registerCallbackFunction([type]() {
            tileTypeEditMode == type ? tileTypeEditMode = "" : tileTypeEditMode = type;
            tileTypeEditMode == type ? highlightSelection = true : highlightSelection = false;
          });
        }
        // store the element in a vector
        _uiElements.emplace_back(std::move(uiElement));
      }
    }
  }
  _tooltip->setVisibility(false);
}

void UIManager::setFPSCounterText(const std::string &fps) { _fpsCounter->setText(fps); }

void UIManager::drawUI()
{
  for (const auto &it : _uiElements)
  {
    if (it->isVisible())
    {
      it->draw();
    }
  }

  if (_showDebugMenu)
  {
    _fpsCounter->draw();
  }
  _tooltip->draw();
}

void UIManager::toggleGroupVisibility(const std::string &groupID)
{
  for (const std::unique_ptr<UiElement> &it : _uiElements)
  {
    if (it->getUiElementData().groupName == groupID)
    {
      it->setVisibility(!it->isVisible());
    }
  }
}

void UIManager::startTooltip(SDL_Event &event, const std::string &tooltipText)
{
  _tooltip->setText(tooltipText);

  _tooltip->setPosition(event.button.x - _tooltip->getUiElementRect().w / 2, event.button.y - _tooltip->getUiElementRect().h);

  _tooltip->startTimer();
}

void UIManager::stopTooltip() { _tooltip->reset(); }

UiElement *UIManager::getUiElementByID(const std::string &UiElementID)
{
  for (auto &it : _uiElements)
  {
    if (it->getUiElementData().elementID == UiElementID)
      return it.get();
  }
  return nullptr;
}
