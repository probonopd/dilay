#include <QCoreApplication>
#include <QDir>
#include <QDomNode>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>
#include <glm/glm.hpp>
#include <unordered_map>
#include "color.hpp"
#include "config.hpp"
#include "macro.hpp"
#include "variant.hpp"
#include "xml-conversion.hpp"

struct Config::Impl {
  typedef Variant <float,int,bool,glm::vec3,glm::ivec2,Color> Value;
  typedef std::unordered_map <std::string, Value>             ConfigMap;

  const std::string optionsFileName;
  const std::string cacheFileName;
  const std::string optionsRoot;
  const std::string cacheRoot;

  std::string optionsFilePath;
  std::string cacheFilePath;
  ConfigMap   optionsMap;
  ConfigMap   cacheMap;

  Impl () 
    : optionsFileName (QCoreApplication::applicationName ().toStdString () + ".config")
    , cacheFileName   (QCoreApplication::applicationName ().toStdString () + ".cache")
    , optionsRoot     ("config")
    , cacheRoot       ("cache")
  {
    optionsFilePath = this->getDirectory () + "/" + this->optionsFileName;
    cacheFilePath   = this->getDirectory () + "/" + this->cacheFileName;

    this->loadFile (false, this->optionsMap);
    this->loadFile (true , this->cacheMap);
  }

  template <class T>
  const T& get (const std::string& relativePath) const {
    assert (relativePath.front () != '/');

    const std::string         absolutePath = "/" + this->optionsRoot + "/" + relativePath;
    ConfigMap::const_iterator value        = this->optionsMap.find (absolutePath);

    if (value == this->optionsMap.end ()) {
      throw (std::runtime_error ("Can not find config path " + absolutePath));
    }
    return value->second.get <T> ();
  }

  template <class T>
  const T& get (const std::string& relativePath, const T& defaultV) const {
    assert (relativePath.front () != '/');

    const std::string         absolutePath = "/" + this->cacheRoot + "/" + relativePath;
    ConfigMap::const_iterator value        = this->cacheMap.find (absolutePath);

    if (value == this->cacheMap.end ()) {
      return defaultV;
    }
    return value->second.get <T> ();
  }

  template <class T>
  void cache (const std::string& relativePath, const T& t) {
    assert (relativePath.front () != '/');

    const std::string absolutePath = "/" + this->cacheRoot + "/" + relativePath;
    Value             value;

    value.set <T>          (t);
    this->cacheMap.erase   (absolutePath);
    this->cacheMap.emplace (absolutePath, value);
  }

  std::string getDirectory () const {
    QString qOptionsFile (this->optionsFileName.c_str ());
    QString dataLocation = QStandardPaths::locate (QStandardPaths::DataLocation, qOptionsFile);

    if (QDir::current ().exists (qOptionsFile)) {
      return QDir::currentPath ().toStdString ();
    }
    else if (! QStandardPaths::locate (QStandardPaths::DataLocation, qOptionsFile).isEmpty ()) {
      return QStandardPaths::writableLocation (QStandardPaths::DataLocation).toStdString ();
    }
    else
      throw (std::runtime_error ("Can not find path that contains configuration file '" + this->optionsFileName + "'"));
  }

  void loadFile (bool isCache, ConfigMap& configMap) {
    QFile file (isCache ? this->cacheFilePath.c_str ()
                        : this->optionsFilePath.c_str () );

    std::string fileName = file.fileName ().toStdString ();

    if (file.open (QIODevice::ReadOnly) == false) {
      if (isCache) {
        return;
      }
      else {
        throw (std::runtime_error ("Can not open configuration file '" + fileName + "'"));
      }
    }
    QDomDocument doc (fileName.c_str ());
    QString      errorMsg;
    int          errorLine   = -1;
    int          errorColumn = -1;
    if (doc.setContent (&file, &errorMsg, &errorLine, &errorColumn) == false) {
      file.close();
      throw (std::runtime_error 
          ( "Error while loading configuration file '" + fileName + "': " + errorMsg.toStdString ()
          + " (" + std::to_string (errorLine) + ","  + std::to_string (errorColumn) + ")"));
    }
    file.close();
    try {
      this->loadConfigMap (configMap, "", doc);
    }
    catch (std::runtime_error& e) {
      throw (std::runtime_error 
          ("Error while parsing configuration file '" + fileName + "': " + e.what ()));
    }
  }

  void loadConfigMap (ConfigMap& configMap, const QString& prefix, QDomNode& node) {
    QDomNode child = node.firstChild ();

    while (child.isNull () == false) {
      if (child.isElement()) {
          QDomElement element   = child.toElement();
          QDomAttr    attribute = element.attributeNode ("type");
          Value       value;

          if (attribute.isNull ()) {
            this->loadConfigMap (configMap, prefix + "/" + element.tagName (), child);
          }
          else if (attribute.value () == "float") {
            this->insertIntoConfigMap <float> (configMap, prefix, element);
          }
          else if (attribute.value () == "integer") {
            this->insertIntoConfigMap <int> (configMap, prefix, element);
          }
          else if (attribute.value () == "boolean") {
            this->insertIntoConfigMap <bool> (configMap, prefix, element);
          }
          else if (attribute.value () == "vector3f") {
            this->insertIntoConfigMap <glm::vec3> (configMap, prefix, element);
          }
          else if (attribute.value () == "vector2i") {
            this->insertIntoConfigMap <glm::ivec2> (configMap, prefix, element);
          }
          else if (attribute.value () == "color") {
            this->insertIntoConfigMap <Color> (configMap, prefix, element);
          }
          else {
            throw (std::runtime_error 
              ( "invalid type '" + attribute.value ().toStdString () + "' "
              + "(" + std::to_string (child.lineNumber   ()) 
              + "," + std::to_string (child.columnNumber ()) + ")"));
          }
      }
      child = child.nextSibling();
    }
  }

  template <typename T>
  bool insertIntoConfigMap (ConfigMap& configMap, const QString& prefix, QDomElement& element) {
    T           t;
    bool        ok  = XmlConversion::fromDomElement (element, t);
    std::string key = (prefix + "/" + element.tagName ()).toStdString ();

    if (ok) {
      Value value;
      value.set <T> (t);
      configMap.emplace (key, value);
    }
    else {
      throw (std::runtime_error 
          ( "can not parse value of key '" + key + "' "
          + "(" + std::to_string (element.lineNumber   ()) 
          + "," + std::to_string (element.columnNumber ()) + ")"));
    }
    return ok;
  }

  void writeCache () {
    QDomDocument doc;

    for (auto& c : this->cacheMap) {
      const std::string& key   = c.first;
            Value&       value = c.second;
            QStringList  path  = QString (key.c_str ()).split ("/", QString::SkipEmptyParts);

      this->appendAsDomChild (doc, doc, path, value);
    }
    if (doc.isNull () == false) {
      QFile file (this->cacheFileName.c_str ());
      if (file.open (QIODevice::WriteOnly)) {
        QTextStream stream (&file);
        doc.save (stream, 2);
        file.close ();
      }
    }
  }

  void appendAsDomChild (QDomDocument& doc, QDomNode& parent, QStringList& path, Value& value) {
    if (path.empty ()) {
      assert (parent.isElement ());
      QDomElement elem = parent.toElement ();
      if (value.is <float> ()) {
        XmlConversion::toDomElement (doc, elem, value.get <float> ());
      }
      else if (value.is <int> ()) {
        XmlConversion::toDomElement (doc, elem, value.get <int> ());
      }
      else if (value.is <bool> ()) {
        XmlConversion::toDomElement (doc, elem, value.get <bool> ());
      }
      else if (value.is <glm::vec3> ()) {
        XmlConversion::toDomElement (doc, elem, value.get <glm::vec3> ());
      }
      else if (value.is <glm::ivec2> ()) {
        XmlConversion::toDomElement (doc, elem, value.get <glm::ivec2> ());
      }
      else if (value.is <Color> ()) {
        XmlConversion::toDomElement (doc, elem, value.get <Color> ());
      }
      else
        std::abort ();
    }
    else {
      const QString& head  = path.first ();
      QDomElement    child = parent.firstChildElement (head);
      if (child.isNull ()) {
        child = doc.createElement (head);
        if (parent.isNull ()) {
          doc.appendChild (child);
        }
        else {
          parent.appendChild (child);
        }
      }
      assert (child.hasAttribute ("type") == false);

      path.removeFirst ();
      this->appendAsDomChild (doc, child, path, value);
    }
  }
};

DELEGATE_BIG2 (Config)
DELEGATE (void, Config, writeCache);

template <class T>
const T& Config :: get (const std::string& path) const {
  return this->impl->get<T> (path);
}

template <class T>
const T& Config :: get (const std::string& path, const T& defaultV) const {
  return this->impl->get<T> (path, defaultV);
}

template <class T>
void Config :: cache (const std::string& path, const T& value) {
  return this->impl->cache<T> (path, value);
}

template const float&      Config :: get<float>        (const std::string&) const;
template const float&      Config :: get<float>        (const std::string&, const float&) const;
template void              Config :: cache<float>      (const std::string&, const float&);
template const int&        Config :: get<int>          (const std::string&) const;
template const int&        Config :: get<int>          (const std::string&, const int&) const;
template void              Config :: cache<int>        (const std::string&, const int&);
template const bool&       Config :: get<bool>         (const std::string&) const;
template const bool&       Config :: get<bool>         (const std::string&, const bool&) const;
template void              Config :: cache<bool>       (const std::string&, const bool&);
template const Color&      Config :: get<Color>        (const std::string&) const;
template const Color&      Config :: get<Color>        (const std::string&, const Color&) const;
template void              Config :: cache<Color>      (const std::string&, const Color&);
template const glm::vec3&  Config :: get<glm::vec3>    (const std::string&) const;
template const glm::vec3&  Config :: get<glm::vec3>    (const std::string&, const glm::vec3&) const;
template void              Config :: cache<glm::vec3>  (const std::string&, const glm::vec3&);
template const glm::ivec2& Config :: get<glm::ivec2>   (const std::string&) const;
template const glm::ivec2& Config :: get<glm::ivec2>   (const std::string&, const glm::ivec2&) const;
template void              Config :: cache<glm::ivec2> (const std::string&, const glm::ivec2&);
