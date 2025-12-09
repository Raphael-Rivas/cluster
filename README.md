# Cluster - Systeme Distribue

## Compilation et Utilisation

### Prerequis
- Compilateur GCC
- Bibliotheque pthread (incluse avec GCC sur Linux)
- Make

### Configuration du cluster

Le fichier `src/nodes.conf` contient la configuration de tous les noeuds du cluster :

```
    0 127.0.0.1 9000
    1 127.0.0.1 9001
    2 127.0.0.1 9002
```

Format : `<node_id> <host> <port>`

### Utilisation

#### Help pour avoir la liste des commandes disponibles:

```bash
    make help
```
#### Exemple d'utilisation:

Terminal 1 :
```bash
    make build
    make run1
```

Terminal 2 :
```bash
    make run2
```

Terminal 3 :
```bash
    make run3
```

## TODO

- envoyer les résultats après node_handle_request_vote dans un attribut votes_received
- question 4 (tick pour savoir qui est le premier à se présenter candidat)
- gestion (victoire) du vote
- tests pour l'étape 2 

## Auteurs

- Cyprien BONS
- Raphaël RIVAS
- Maël NICOLAS
- Killian RAMUS