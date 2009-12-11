/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

template< typename DataType>
inline
void
MonitorDataStorage::update( const DataType&, bool)
{
  // Error condition.
  ACE_ERROR((LM_ERROR,
    ACE_TEXT("(%P|%t) ERROR: MonitorDataStorage::update() - ")
    ACE_TEXT("recieved sample of unknown data type.\n")
  ));
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::ServiceParticipantReport>(
  const OpenDDS::DCPS::ServiceParticipantReport& data,
  bool remove
)
{
  // struct ServiceParticipantReport {
  //   string    host;
  //   long      pid;
  //   GUIDSeq   domain_participants;
  //   ULongSeq  transports;
  //   NVPSeq    values;
  // };

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s ServiceParticipantReport, host: %C, pid: %d.\n"),
    remove? "removing": "processing",
    (const char*)data.host,
    data.pid
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = false;
  bool dataChanged   = false;
  bool create        = !remove;

  // Obtain, or possibly create, the host and PID nodes.
  std::string host( data.host);
  ProcessKey pid( host, data.pid);
  TreeNode* pidNode = this->getProcessNode( pid, create);
  if( !pidNode) {
    return;
  }
  layoutChanged |= create;

  if( remove) {
    // Descend from the pidNode and remove it and all its children from
    // the maps.
    this->cleanMaps( pidNode);
    this->processToTreeMap_.erase( pid);

    TreeNode* hostNode = pidNode->parent();
    hostNode->removeChildren( pidNode->row(), 1);

    // Check and remove the host node if there are no pid nodes remaining
    // after the removal (no children).
    if( hostNode->size() == 0) {
      this->hostToTreeMap_.erase( host);
      delete hostNode;
    }

    // Nothing else to do on removal.
    this->model_->changed();
    return;
  }

  // PARTICIPANTS
  // NOTE: The following makes sure that any new DomainParticipants are
  //       added to the host/pid as they are received by this update.  It
  //       does *not* remove any deleted participants.  This is left for
  //       the DomainParticipantReport updates.
  int size = data.domain_participants.length();
  for( int index = 0; index < size; ++index) {
    create = true;
    (void)this->getParticipantNode(
      pid,
      data.domain_participants[ index],
      create
    );
    layoutChanged |= create;
  }

  // TRANSPORTS
  size = data.transports.length();
  for( int index = 0; index < size; ++index) {
    create = true;
    int transport = data.transports[ index];
    TransportKey key( host, data.pid, transport);
    (void)this->getTransportNode( key, create);
    layoutChanged |= create;
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( pidNode, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::DomainParticipantReport>(
  const OpenDDS::DCPS::DomainParticipantReport& data,
  bool remove
)
{
  //  struct DomainParticipantReport {
  //    string           host;
  //    long             pid;
  //    GUID_t           dp_id;
  //    DDS::DomainId_t  domain_id;
  //    GUIDSeq          topics;
  //    NVPSeq           values;
  //  };

  OpenDDS::DCPS::GuidConverter converter( data.dp_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s DomainParticipantReport, id: %C, domain: data.domain_id.\n"),
    remove? "removing": "processing",
    std::string(converter).c_str(),
    data.domain_id
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = !remove; // Updated by getParticipantNode()
  bool dataChanged   = false;

  std::string host( data.host);
  ProcessKey pid( host, data.pid);
  TreeNode* node = this->getParticipantNode( pid, data.dp_id, layoutChanged);
  if( !node) {
    return;
  }

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->guidToTreeMap_.erase( data.dp_id);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // Domain Id value.
  QString label( QObject::tr( "Domain Id"));
  int row = node->indexOf( 0, label);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << label << QString::number( data.domain_id);
    TreeNode* domainNode = new TreeNode( list, node);
    node->append( domainNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    TreeNode* domainNode = (*node)[ row];
    domainNode->setData( 1, QString::number( data.domain_id));
    dataChanged = true;
  }

  // TOPICS
  // NOTE: The following makes sure that any new Topics are added to
  //       the DomainParticipant as they are received by this update.
  //       It does *not* remove any deleted topics.  This is left for
  //       the TopicReport updates.
  int size = data.topics.length();
  for( int index = 0; index < size; ++index) {
    bool create = true;
    (void)this->getNode(
      std::string( "Topic"),
      data.dp_id,
      data.topics[ index],
      create
    );
    layoutChanged |= create;
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::TopicReport>(
  const OpenDDS::DCPS::TopicReport& data,
  bool remove
)
{
  //  struct TopicReport {
  //    GUID_t  dp_id;
  //    GUID_t  topic_id;
  //    string  topic_name;
  //    string  type_name;
  //    NVPSeq  values;
  //  };

  OpenDDS::DCPS::GuidConverter converter( data.topic_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s TopicReport, id: %C, name: %C, type: %C.\n"),
    remove? "removing": "processing",
    std::string(converter).c_str(),
    (const char*)data.topic_name,
    (const char*)data.type_name
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = !remove; // Updated by getNode()
  bool dataChanged   = false;

  // Find or create this topic node.
  TreeNode* node = this->getNode(
                     std::string("Topic"),
                     data.dp_id,
                     data.topic_id,
                     layoutChanged
                   );
  if( !node) {
    return;
  }

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->guidToTreeMap_.erase( data.dp_id);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // Topic name value.
  QString nameLabel( QObject::tr( "Topic Name"));
  int row = node->indexOf( 0, nameLabel);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << nameLabel
         << QString( QObject::tr( static_cast<const char*>(data.topic_name)));
    TreeNode* nameNode = new TreeNode( list, node);
    node->append( nameNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    TreeNode* nameNode = (*node)[ row];
    nameNode->setData(
      1,
      QString( QObject::tr( static_cast<const char*>(data.topic_name)))
    );
    dataChanged = true;
  }

  // Data type value.
  QString typeLabel( QObject::tr( "Data Type"));
  row = node->indexOf( 0, typeLabel);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << typeLabel
         << QString( QObject::tr( static_cast<const char*>(data.type_name)));
    TreeNode* typeNode = new TreeNode( list, node);
    node->append( typeNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    TreeNode* typeNode = (*node)[ row];
    typeNode->setData(
      1,
      QString( QObject::tr( static_cast<const char*>(data.type_name)))
    );
    dataChanged = true;
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::PublisherReport>(
  const OpenDDS::DCPS::PublisherReport& data,
  bool remove
)
{
  // struct PublisherReport {
  //   DDS::InstanceHandle_t handle;
  //   GUID_t        dp_id;
  //   unsigned long transport_id;
  //   GUIDSeq       writers;
  //   NVPSeq        values;
  // };

  OpenDDS::DCPS::GuidConverter converter( data.dp_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s PublisherReport, id: %C, handle: %d, transport: 0x%x.\n"),
    remove? "removing": "processing",
    std::string(converter).c_str(),
    data.handle,
    data.transport_id
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = !remove; // Updated by getEndpointNode()
  bool dataChanged   = false;

  InstanceKey key( data.dp_id, data.handle);
  TreeNode* node = this->getInstanceNode(
                     std::string( "Publisher"),
                     key,
                     layoutChanged
                   );
  if( !node) {
    return;
  }

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->instanceToTreeMap_.erase( key);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // Transport Id value.
  QString label( QObject::tr( "Transport Id"));
  QString value = QString("0x%1")
                  .arg( data.transport_id, 8, 16, QLatin1Char('0'));
  int row = node->indexOf( 0, label);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << label << value;
    TreeNode* idNode = new TreeNode( list, node);
    node->append( idNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    TreeNode* idNode = (*node)[ row];
    idNode->setData( 1, value);
    dataChanged = true;
  }

  // WRITERS
  // NOTE: The following makes sure that any new DataWriters are added to
  //       the Publisher as they are received by this update.  It does
  //       *not* remove any deleted writers.  This is left for the
  //       DataWriterReport updates.
  int size = data.writers.length();
  for( int index = 0; index < size; ++index) {
    bool create = true;
    (void)this->getEndpointNode(
      std::string( "Writer"),
      key,
      data.writers[ index],
      create
    );
    layoutChanged |= create;
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::SubscriberReport>(
  const OpenDDS::DCPS::SubscriberReport& data,
  bool remove
)
{
  // struct SubscriberReport {
  //   DDS::InstanceHandle_t handle;
  //   GUID_t        dp_id;
  //   unsigned long transport_id;
  //   GUIDSeq       readers;
  //   NVPSeq        values;
  // };

  OpenDDS::DCPS::GuidConverter converter( data.dp_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s SubscriberReport, id: %C, handle: %d, transport: 0x%x.\n"),
    remove? "removing": "processing",
    std::string(converter).c_str(),
    data.handle,
    data.transport_id
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = !remove; // Updated by getEndpointNode()
  bool dataChanged   = false;

  InstanceKey key( data.dp_id, data.handle);
  TreeNode* node = this->getInstanceNode(
                     std::string( "Subscriber"),
                     key,
                     layoutChanged
                   );
  if( !node) {
    return;
  }

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->instanceToTreeMap_.erase( key);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // Transport Id value.
  QString label( QObject::tr( "Transport Id"));
  QString value = QString("0x%1")
                  .arg( data.transport_id, 8, 16, QLatin1Char('0'));
  int row = node->indexOf( 0, label);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << label << value;
    TreeNode* idNode = new TreeNode( list, node);
    node->append( idNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    TreeNode* idNode = (*node)[ row];
    idNode->setData( 1, value);
    dataChanged = true;
  }

  // READERS
  // NOTE: The following makes sure that any new DataReaders are added to
  //       the Subscriber as they are received by this update.  It does
  //       *not* remove any deleted writers.  This is left for the
  //       DataReaderReport updates.
  int size = data.readers.length();
  for( int index = 0; index < size; ++index) {
    bool create = true;
    (void)this->getEndpointNode(
      std::string( "Reader"),
      key,
      data.readers[ index],
      create
    );
    layoutChanged |= create;
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::DataWriterReport>(
  const OpenDDS::DCPS::DataWriterReport& data,
  bool remove
)
{
  //  struct DataWriterAssociation {
  //    GUID_t        dr_id;
  //  };
  //  typedef sequence<DataWriterAssociation> DWAssociations;
  //  struct DataWriterReport {
  //    GUID_t                 dp_id;
  //    DDS::InstanceHandle_t  pub_handle;
  //    GUID_t                 dw_id;
  //    GUID_t                 topic_id;
  //    DDS::InstanceHandleSeq instances;
  //    DWAssociations         associations;
  //    NVPSeq                 values;
  //  };

  OpenDDS::DCPS::GuidConverter idconverter( data.dw_id);
  OpenDDS::DCPS::GuidConverter topicconverter( data.topic_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s DataWriterReport, id: %C, topic: %C.\n"),
    remove? "removing": "processing",
    std::string(idconverter).c_str(),
    std::string(topicconverter).c_str()
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool create        = !remove;
  bool layoutChanged = false;
  bool dataChanged   = false;

  InstanceKey key( data.dp_id, data.pub_handle);
  TreeNode* node = this->getEndpointNode(
                     std::string( "Writer"),
                     key,
                     data.dw_id,
                     create
                   );
  if( !node) {
    return;
  }
  layoutChanged |= create;

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->guidToTreeMap_.erase( data.dw_id);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // TOPIC
  // N.B. This topic id value is for reference and are not connected to
  //      the information containing Topic nodes which are children of
  //      the DomainParticipant nodes.  The values should correspond,
  //      but we do not duplicate the data here.
  // N.B. A consequence of this is that out-of-order processing, where
  //      the DataWriterReport is processed prior to the TopicReport will
  //      result in the Topic GUID being displayed instead of the Topic
  //      name.  There is no mechanism currently to update this value
  //      when the TopicReport is received.

  // Find the actual topic, if possible.  Use the name as the topic
  // value if it can be found, or the string-ified GUID if not.
  OpenDDS::DCPS::GuidConverter converter( data.topic_id);
  QString topicValue( std::string(converter).c_str());
  TreeNode* topicNode = this->getNode(
                          std::string( "Topic"),
                          data.dp_id,
                          data.topic_id,
                          create
                        );
  if( topicNode) {
    QString nameLabel( QObject::tr( "Topic Name"));
    int row = topicNode->indexOf( 0, nameLabel);
    if( row != -1) {
      topicValue = (*topicNode)[ row]->column( 1).toString();
    }
  }

  // Now find or create a child node to hold the topic name.
  QString topicLabel( QObject::tr( "Topic"));
  int row = node->indexOf( 0, topicLabel);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << topicLabel << topicValue;
    TreeNode* valueNode = new TreeNode( list, node);
    node->append( valueNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    (*node)[ row]->setData( 1, topicValue);
    dataChanged = true;
  }

  // ASSOCIATIONS
  int size = data.associations.length();
  for( int index = 0; index < size; ++index) {
    // Create a child node to hold the association if its not already in
    // the tree.
    OpenDDS::DCPS::GuidConverter converter( data.associations[ index].dr_id);
    QString reader( std::string(converter).c_str());
    int row = node->indexOf( 1, reader);
    if( row == -1) {
      // New data, insert.
      QList<QVariant> list;
      list << QString( QObject::tr("Reader")) << reader;
      TreeNode* valueNode = new TreeNode( list, node);
      node->append( valueNode);
      layoutChanged = true;
    }
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::DataWriterPeriodicReport>(
  const OpenDDS::DCPS::DataWriterPeriodicReport& data,
  bool remove
)
{
  // struct DataWriterAssociationPeriodic {
  //   GUID_t        dr_id;
  //   unsigned long sequence_number;
  // };
  // typedef sequence<DataWriterAssociationPeriodic> DWAssociationsPeriodic;
  // struct DataWriterPeriodicReport {
  //   GUID_t        dw_id;
  //   unsigned long data_dropped_count;
  //   unsigned long data_delivered_count;
  //   unsigned long control_dropped_count;
  //   unsigned long control_delivered_count;
  //   DWAssociationsPeriodic associations;
  //   NVPSeq        values;
  // };

  OpenDDS::DCPS::GuidConverter converter( data.dw_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s DataWriterPeriodicReport, id: %C.\n"),
    remove? "removing": "processing",
    std::string(converter).c_str()
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = false;
  bool dataChanged   = false;

  TreeNode* node = 0;

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::DataReaderReport>(
  const OpenDDS::DCPS::DataReaderReport& data,
  bool remove
)
{
  //  struct DataReaderAssociation {
  //    GUID_t        dw_id;
  //    short         state;
  //  };
  //  typedef sequence<DataReaderAssociation> DRAssociations;
  //  struct DataReaderReport {
  //    GUID_t                 dp_id;
  //    DDS::InstanceHandle_t  sub_handle;
  //    GUID_t                 dr_id;
  //    GUID_t                 topic_id;
  //    DDS::InstanceHandleSeq instances;
  //    DRAssociations         associations;
  //    NVPSeq                 values;
  //  };

  OpenDDS::DCPS::GuidConverter idconverter( data.dr_id);
  OpenDDS::DCPS::GuidConverter topicconverter( data.topic_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s DataReaderReport, id: %C, topic: %C.\n"),
    remove? "removing": "processing",
    std::string(idconverter).c_str(),
    std::string(topicconverter).c_str()
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool create        = !remove;
  bool layoutChanged = false;
  bool dataChanged   = false;

  InstanceKey key( data.dp_id, data.sub_handle);
  TreeNode* node = this->getEndpointNode(
                     std::string( "Reader"),
                     key,
                     data.dr_id,
                     create
                   );
  if( !node) {
    return;
  }
  layoutChanged |= create;

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->guidToTreeMap_.erase( data.dr_id);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // TOPIC
  // N.B. This topic id value is for reference and are not connected to
  //      the information containing Topic nodes which are children of
  //      the DomainParticipant nodes.  The values should correspond,
  //      but we do not duplicate the data here.
  // N.B. A consequence of this is that out-of-order processing, where
  //      the DataWriterReport is processed prior to the TopicReport will
  //      result in the Topic GUID being displayed instead of the Topic
  //      name.  There is no mechanism currently to update this value
  //      when the TopicReport is received.

  // Find the actual topic, if possible.  Use the name as the topic
  // value if it can be found, or the string-ified GUID if not.
  OpenDDS::DCPS::GuidConverter converter( data.topic_id);
  QString topicValue( std::string(converter).c_str());
  TreeNode* topicNode = this->getNode(
                          std::string( "Topic"),
                          data.dp_id,
                          data.topic_id,
                          create
                        );
  if( topicNode) {
    QString nameLabel( QObject::tr( "Topic Name"));
    int row = topicNode->indexOf( 0, nameLabel);
    if( row != -1) {
      topicValue = (*topicNode)[ row]->column( 1).toString();
    }
  }

  // Now find or create a child node to hold the topic name.
  QString topicLabel( QObject::tr( "Topic"));
  int row = node->indexOf( 0, topicLabel);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << topicLabel << topicValue;
    TreeNode* valueNode = new TreeNode( list, node);
    node->append( valueNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    (*node)[ row]->setData( 1, topicValue);
    dataChanged = true;
  }

  // ASSOCIATIONS
  int size = data.associations.length();
  for( int index = 0; index < size; ++index) {
    // Create a child node to hold the association if its not already in
    // the tree.
    OpenDDS::DCPS::GuidConverter converter( data.associations[ index].dw_id);
    QString writer( std::string(converter).c_str());
    int row = node->indexOf( 1, writer);
    if( row == -1) {
      // New data, insert.
      QList<QVariant> list;
      list << QString( QObject::tr("Writer")) << writer;
      TreeNode* valueNode = new TreeNode( list, node);
      node->append( valueNode);
      layoutChanged = true;
    }
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::DataReaderPeriodicReport>(
  const OpenDDS::DCPS::DataReaderPeriodicReport& data,
  bool remove
)
{
  // struct DataReaderAssociationPeriodic {
  //   GUID_t        dw_id;
  //   unsigned long samples_available;
  //   // Stats      latency_stats;
  // };
  // typedef sequence<DataReaderAssociationPeriodic> DRAssociationsPeriodic;
  // struct DataReaderPeriodicReport {
  //   GUID_t        dr_id;
  //   DRAssociationsPeriodic associations;
  //   NVPSeq        values;
  // };

  OpenDDS::DCPS::GuidConverter converter( data.dr_id);
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s DataReaderPeriodicReport, id: %C.\n"),
    remove? "removing": "processing",
    std::string(converter).c_str()
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = false;
  bool dataChanged   = false;

  TreeNode* node = 0;

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

template<>
inline
void
MonitorDataStorage::update< OpenDDS::DCPS::TransportReport>(
  const OpenDDS::DCPS::TransportReport& data,
  bool remove
)
{
  // struct TransportReport {
  //   string        host;
  //   long          pid;
  //   unsigned long transport_id;
  //   string        transport_type;
  //   NVPSeq        values;
  // };

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) MonitorDataStorage::update() - ")
    ACE_TEXT("%s TransportReport, host: %C, pid: %d, ")
    ACE_TEXT("transport: 0x%x, type: %C.\n"),
    remove? "removing": "processing",
    (const char*)data.host,
    data.pid,
    data.transport_id,
    (const char*)data.transport_type
  ));

  // Retain knowledge of node insertions, updates, and deletions.
  bool layoutChanged = !remove; // Updated by getTransportNode()
  bool dataChanged   = false;

  TransportKey key( std::string(data.host), data.pid, data.transport_id);
  TreeNode* node = this->getTransportNode( key, layoutChanged);
  if( !node) {
    return;
  }

  if( remove) {
    // Descend from the node and remove it and all its children from
    // the maps.
    this->cleanMaps( node);
    this->transportToTreeMap_.erase( key);
    TreeNode* parent = node->parent();
    if( parent) {
      parent->removeChildren( node->row(), 1);

    } else {
      delete node;
    }

    // Nothing else to do on removal, let the GUI know we changed the
    // model.
    this->model_->changed();
    return;
  }

  // Transpot type value.
  QString typeLabel( QObject::tr( "Type"));
  int row = node->indexOf( 0, typeLabel);
  if( row == -1) {
    // New data, insert.
    QList<QVariant> list;
    list << typeLabel
         << QString( QObject::tr( static_cast<const char*>(data.transport_type)));
    TreeNode* nameNode = new TreeNode( list, node);
    node->append( nameNode);
    layoutChanged = true;

  } else {
    // Existing data, update.
    TreeNode* nameNode = (*node)[ row];
    nameNode->setData(
      1,
      QString( QObject::tr( static_cast<const char*>(data.transport_type)))
    );
    dataChanged = true;
  }

  // NAME / VALUE DATA, notify GUI of changes.
  this->displayNvp( node, data.values, layoutChanged, dataChanged);
}

