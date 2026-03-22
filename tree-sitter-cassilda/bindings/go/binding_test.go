package tree_sitter_cassilda_test

import (
	"testing"

	tree_sitter "github.com/smacker/go-tree-sitter"
	"github.com/tree-sitter/tree-sitter-cassilda"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_cassilda.Language())
	if language == nil {
		t.Errorf("Error loading Cassilda grammar")
	}
}
